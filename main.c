#include <libk/string.h>
#include <libk/stdio.h>
#include <libk/ctype.h>
#include <vitasdk.h>
#include <taihen.h>
#include <kuio.h>

#define HOOKS_NUM 2       // Hooked functions num
#define CHUNK_SIZE 2048   // Buffer size in bytes

// Debug
#define DEBUG_DUMP_TEXTURES 0
#define DEBUG_DUMP_SHADERS  1
#define DEBUG_DUMP_MODELS   0

static uint8_t current_hook;
static SceUID hooks[HOOKS_NUM];
static tai_hook_ref_t refs[HOOKS_NUM];
static char fname[256];
static char titleid[16];
static uint8_t buffer[CHUNK_SIZE];

// Simplified generic hooking function
void hookFunction(uint32_t nid, const void *func){
    hooks[current_hook] = taiHookFunctionImport(&refs[current_hook],TAI_MAIN_MODULE,TAI_ANY_LIBRARY,nid,func);
    current_hook++;
}

int tex_format_to_bytespp(SceGxmTextureFormat format){
    switch (format & 0x9f000000U) {
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
            return 1;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
        case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
        case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
        case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
            return 2;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
            return 3;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
        case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
        case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
        default:
            return 4;
    }
}

static uint64_t fnv1a(const void *data, size_t size) {
    uint8_t* begin = (uint8_t*)data;
    uint8_t* end = begin + (size-1);
    uint64_t result = 0xcbf29ce484222325;
    uint8_t* p;
    
    for (p = begin; p != end; ++p) {
        result ^= *p;
        result *= 0x100000001b3;
    }

    return result;
}

int sceGxmSetFragmentTexture_patched(SceGxmContext *context, unsigned int textureIndex, const SceGxmTexture *texture){
    
    if (!DEBUG_DUMP_TEXTURES) return TAI_CONTINUE(int, refs[0], context, textureIndex, texture);
    
    // Getting texture bpp and stride
    uint32_t tex_format = sceGxmTextureGetFormat(texture);
    uint8_t bpp = tex_format_to_bytespp(tex_format);
    uint32_t width = sceGxmTextureGetWidth(texture);
    uint32_t height = sceGxmTextureGetHeight(texture);
    uint32_t stride = sceGxmTextureGetStride(texture);
    if (stride == 0) stride = ((width + 7) & ~7) * bpp;
    
    // Getting texture data offset
    uint8_t* ptr = (uint8_t*)sceGxmTextureGetData(texture);
    
    // Checking if texture had already been dumped
    SceUID fd;
    sprintf(fname, "ux0:data/GxmDumper/%s/textures/%llu_0x%lX.bmp", titleid, fnv1a(ptr, stride * height), tex_format);
    kuIoOpen(fname, SCE_O_RDONLY, &fd);
    
    // Not dumped, dumping it
    if (fd < 0){
        
        // Locking home button
        sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN | SCE_SHELL_UTIL_LOCK_TYPE_QUICK_MENU);
        
        // Opening file in write mode
        kuIoOpen(fname, SCE_O_CREAT | SCE_O_WRONLY, &fd);
        
        // Writing Bitmap Header
        memset(buffer, 0, 0x36);
        *((uint16_t*)(&buffer[0x00])) = 0x4D42;
        *((uint32_t*)(&buffer[0x02])) = ((width*height)<<2)+0x36;
        *((uint32_t*)(&buffer[0x0A])) = 0x36;
        *((uint32_t*)(&buffer[0x0E])) = 0x28;
        *((uint32_t*)(&buffer[0x12])) = width;
        *((uint32_t*)(&buffer[0x16])) = height;
        *((uint32_t*)(&buffer[0x1A])) = 0x00200001;
        *((uint32_t*)(&buffer[0x22])) = ((width*height)<<2);
        kuIoWrite(fd, buffer, 0x36);
        
        // Writing Bitmap Table
        int x, y, i;
        i = 0;
        uint32_t* tbuffer = (uint32_t*)buffer;
        for (y = 1; y<=height; y++){
            for (x = 0; x<width; x++){
                uint8_t* clr = &ptr[x*bpp+(height-y)*stride];
                uint8_t r = clr[0];
                uint8_t g = clr[1];
                uint8_t b = clr[2];
                uint8_t a = (bpp == 3 ? 0xFF : clr[3]);
                tbuffer[i] = (a<<24) | (r<<16) | (g<<8) | b;
                i++;
                if (i == (CHUNK_SIZE>>2)){
                    i = 0;
                    kuIoWrite(fd, buffer, CHUNK_SIZE);
                }
            }
        }
        if (i != 0) kuIoWrite(fd, buffer, i<<2);
        
        // Unlocking home button
        sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN | SCE_SHELL_UTIL_LOCK_TYPE_QUICK_MENU);
    
    }
    
    // Closing file
    kuIoClose(fd);
    
    return TAI_CONTINUE(int, refs[0], context, textureIndex, texture);
}

int sceGxmShaderPatcherRegisterProgram_patched(SceGxmShaderPatcher *shaderPatcher, const SceGxmProgram *programHeader, SceGxmShaderPatcherId *programId){
    
    int res = TAI_CONTINUE(int, refs[1], shaderPatcher, programHeader, programId);
    if (!DEBUG_DUMP_SHADERS) return res;
    
    // Getting shader type and size
    unsigned int size = sceGxmProgramGetSize(programHeader);
    
    // Checking if shader had already been dumped
    SceUID fd;
    sprintf(fname, "ux0:data/GxmDumper/%s/shaders/%llu.gxp", titleid, fnv1a(programHeader, size));
    kuIoOpen(fname, SCE_O_RDONLY, &fd);
    
    // Not dumped, dumping it
    if (fd < 0){
        
        // Locking home button
        sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN | SCE_SHELL_UTIL_LOCK_TYPE_QUICK_MENU);
        
        // Opening file in write mode
        kuIoOpen(fname, SCE_O_CREAT | SCE_O_WRONLY, &fd);
        
        // Writing gxp file
        unsigned int i = 0;
        uint8_t* ptr = (uint8_t*)programHeader;
        while (i < size){
            if (i + CHUNK_SIZE < size) kuIoWrite(fd, &ptr[i], CHUNK_SIZE);
            else kuIoWrite(fd, &ptr[i], size - i);
            i += CHUNK_SIZE;
        }
        
        // Unlocking home button
        sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN | SCE_SHELL_UTIL_LOCK_TYPE_QUICK_MENU);
    
    }
    
    // Closing file
    kuIoClose(fd);
    
    return res;
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args) {
    
    // Getting game Title ID
    sceAppMgrAppParamGetString(0, 12, titleid , 256);
    
    // Initing sceShellUtil for home button locking
    sceShellUtilInitEvents(0);
    
    // Just in case the folder doesn't exist
    kuIoMkdir("ux0:data/GxmDumper");
    sprintf(fname, "ux0:data/GxmDumper/%s", titleid);
    kuIoMkdir(fname);
    sprintf(fname, "ux0:data/GxmDumper/%s/textures", titleid);
    kuIoMkdir(fname);
    sprintf(fname, "ux0:data/GxmDumper/%s/shaders", titleid);
    kuIoMkdir(fname);
    sprintf(fname, "ux0:data/GxmDumper/%s/models", titleid);
    kuIoMkdir(fname);
    
    // Hooking functions
    hookFunction(0x29C34DF5, sceGxmSetFragmentTexture_patched);
    hookFunction(0x2B528462, sceGxmShaderPatcherRegisterProgram_patched);
    
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    
    // Freeing hooks
    while (current_hook-- > 0){
        taiHookRelease(hooks[current_hook], refs[current_hook]);
    }
    
    return SCE_KERNEL_STOP_SUCCESS;    
}
