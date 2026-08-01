// ImGui bgfx backend - ImGui 1.92+ uyumlu
// ImGui 1.92, ImTextureData tabanli yeni bir texture sistemi kullaniyor.
// Backend; WantCreate/WantUpdates/WantDestroy durumlarini NewFrame()'de islemeli.

#include "imgui_impl_bgfx.h"
#include <imgui.h>
#include <bgfx/bgfx.h>
#include <bx/timer.h>
#include <bx/math.h>
#include <cstdio>
#include <cstring>
#include <bgfx/embedded_shader.h>

#include <imgui/vs_ocornut_imgui.bin.h>
#include <imgui/fs_ocornut_imgui.bin.h>

static const bgfx::EmbeddedShader s_embeddedShaders[] =
{
    BGFX_EMBEDDED_SHADER(vs_ocornut_imgui),
    BGFX_EMBEDDED_SHADER(fs_ocornut_imgui),
    BGFX_EMBEDDED_SHADER_END()
};

struct ImGui_ImplBgfx_Data {
    bgfx::VertexLayout  layout;
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle uniformTexture = BGFX_INVALID_HANDLE;
    uint8_t viewId = 0;
};

static ImGui_ImplBgfx_Data* ImGui_ImplBgfx_GetBackendData() {
    return ImGui::GetCurrentContext()
        ? (ImGui_ImplBgfx_Data*)ImGui::GetIO().BackendRendererUserData
        : nullptr;
}

// --- ImTextureData'dan bgfx texture handle'i al/ata ---
static bgfx::TextureHandle GetOrCreateBgfxTexture(ImTextureData* tex) {
    // BackendUserData'yi bgfx texture index'i olarak kullaniyoruz
    // BGFX_INVALID_HANDLE.idx = 0xFFFF, bu yuzden NULL ile karismasini engelleyelim
    if (tex->BackendUserData != nullptr) {
        bgfx::TextureHandle h;
        h.idx = (uint16_t)(uintptr_t)tex->BackendUserData;
        return h;
    }
    return BGFX_INVALID_HANDLE;
}

// --- Texture olustur / guncelle ---
static void HandleTextureCreate(ImTextureData* tex) {
    const bgfx::Memory* mem = bgfx::copy(tex->Pixels, tex->Width * tex->Height * tex->BytesPerPixel);

    bgfx::TextureFormat::Enum fmt = (tex->Format == ImTextureFormat_Alpha8)
        ? bgfx::TextureFormat::R8
        : bgfx::TextureFormat::RGBA8;

    bgfx::TextureHandle handle = bgfx::createTexture2D(
        (uint16_t)tex->Width, (uint16_t)tex->Height, false, 1,
        fmt,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        mem
    );

    if (!bgfx::isValid(handle)) {
        printf("[BgfxBackend] ERROR: Texture creation failed (UniqueID=%d)!\n", tex->UniqueID);
        return;
    }

    printf("[BgfxBackend] Created texture: UniqueID=%d, %dx%d, handle.idx=%d\n",
        tex->UniqueID, tex->Width, tex->Height, (int)handle.idx);

    // BackendUserData'ye handle.idx'i kaydet
    tex->BackendUserData = (void*)(uintptr_t)handle.idx;
    tex->SetTexID((ImTextureID)(uintptr_t)handle.idx);
    tex->SetStatus(ImTextureStatus_OK);
}

static void HandleTextureUpdate(ImTextureData* tex) {
    bgfx::TextureHandle handle = GetOrCreateBgfxTexture(tex);
    if (!bgfx::isValid(handle)) {
        // Henuz olustuyurulmamis, olustur
        HandleTextureCreate(tex);
        return;
    }

    // Guncelleme gerekiyor: texture'i yeniden yukle
    const bgfx::Memory* mem = bgfx::copy(
        tex->Pixels, tex->Width * tex->Height * tex->BytesPerPixel
    );
    bgfx::updateTexture2D(handle, 0, 0, 0, 0, (uint16_t)tex->Width, (uint16_t)tex->Height, mem);
    tex->SetStatus(ImTextureStatus_OK);
}

static void HandleTextureDestroy(ImTextureData* tex) {
    bgfx::TextureHandle handle = GetOrCreateBgfxTexture(tex);
    if (bgfx::isValid(handle)) {
        bgfx::destroy(handle);
        printf("[BgfxBackend] Destroyed texture: UniqueID=%d, handle.idx=%d\n",
            tex->UniqueID, (int)handle.idx);
    }
    tex->BackendUserData = nullptr;
    tex->SetTexID(ImTextureID_Invalid);
    tex->SetStatus(ImTextureStatus_Destroyed);
}

// --- NewFrame: ImGui'nin texture isteklerini isle ---
static void ProcessTextureUpdates(ImDrawData* drawData) {
    if (!drawData || !drawData->Textures) return;

    // ImGui'nin texture update listesini tara (Render sirasinda)
    for (ImTextureData* tex : *drawData->Textures) {
        if (tex->Status == ImTextureStatus_WantCreate) {
            HandleTextureCreate(tex);
        } else if (tex->Status == ImTextureStatus_WantUpdates) {
            HandleTextureUpdate(tex);
        } else if (tex->Status == ImTextureStatus_WantDestroy) {
            HandleTextureDestroy(tex);
        }
    }
}

// --- Render: texture handle'i dogru sekilde al ---
static bgfx::TextureHandle GetRenderTexture(ImTextureID texID) {
    bgfx::TextureHandle h;
    h.idx = (uint16_t)(uintptr_t)texID;
    return h;
}

bool ImGui_ImplBgfx_Init(uint8_t viewId) {
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "imgui_impl_bgfx";
    // ImGui 1.92+ icin kritik flag'ler:
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures; // Yeni texture sistemini aktive eder

    ImGui_ImplBgfx_Data* bd = new ImGui_ImplBgfx_Data();
    io.BackendRendererUserData = (void*)bd;
    bd->viewId = viewId;

    bd->layout.begin()
        .add(bgfx::Attrib::Position,  2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
        .end();

    bd->uniformTexture = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);

    bgfx::RendererType::Enum type = bgfx::getRendererType();
    bgfx::ShaderHandle vs = bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_ocornut_imgui");
    bgfx::ShaderHandle fs = bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_ocornut_imgui");

    if (!bgfx::isValid(vs) || !bgfx::isValid(fs)) {
        printf("[BgfxBackend] ERROR: Shader creation failed for renderer type %d!\n", (int)type);
    }

    bd->program = bgfx::createProgram(vs, fs, true);

    if (!bgfx::isValid(bd->program)) {
        printf("[BgfxBackend] ERROR: Shader program creation failed!\n");
    } else {
        printf("[BgfxBackend] Shader program created OK.\n");
    }

    return bgfx::isValid(bd->program);
}

void ImGui_ImplBgfx_Shutdown() {
    ImGui_ImplBgfx_Data* bd = ImGui_ImplBgfx_GetBackendData();
    if (!bd) return;

    // Tum texture'lari temizle
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    for (ImTextureData* tex : pio.Textures) {
        if (tex->BackendUserData != nullptr) {
            HandleTextureDestroy(tex);
        }
    }

    if (bgfx::isValid(bd->uniformTexture)) bgfx::destroy(bd->uniformTexture);
    if (bgfx::isValid(bd->program))        bgfx::destroy(bd->program);

    delete bd;
    ImGui::GetIO().BackendRendererUserData = nullptr;
}

void ImGui_ImplBgfx_NewFrame() {
    // Texture guncellemeleri artik RenderDrawData() icinde yapiliyor.
}

void ImGui_ImplBgfx_RenderDrawData(ImDrawData* drawData) {
    ImGui_ImplBgfx_Data* bd = ImGui_ImplBgfx_GetBackendData();
    if (!bd) return;

    // 1.92+ Texture yuklemelerini isliyoruz. BU ONEMLIDIR! 
    // Yoksa komut islenirken GetTexID() assertion verir.
    ProcessTextureUpdates(drawData);

    if (drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f)
        return;

    float ortho[16];
    bx::mtxOrtho(ortho,
        drawData->DisplayPos.x,
        drawData->DisplayPos.x + drawData->DisplaySize.x,
        drawData->DisplayPos.y + drawData->DisplaySize.y,
        drawData->DisplayPos.y,
        0.0f, 1000.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(bd->viewId, nullptr, ortho);
    bgfx::setViewRect(bd->viewId, 0, 0,
        (uint16_t)drawData->DisplaySize.x, (uint16_t)drawData->DisplaySize.y);

    for (int n = 0; n < drawData->CmdListsCount; n++) {
        const ImDrawList* cmdList = drawData->CmdLists[n];
        uint32_t numVtx = (uint32_t)cmdList->VtxBuffer.Size;
        uint32_t numIdx = (uint32_t)cmdList->IdxBuffer.Size;

        bgfx::TransientVertexBuffer tvb;
        bgfx::TransientIndexBuffer tib;

        if (!bgfx::allocTransientBuffers(&tvb, bd->layout, numVtx, &tib, numIdx))
            break;

        std::memcpy(tvb.data, cmdList->VtxBuffer.Data, numVtx * sizeof(ImDrawVert));
        std::memcpy(tib.data, cmdList->IdxBuffer.Data, numIdx * sizeof(ImDrawIdx));

        uint32_t idxOffset = 0;
        for (const ImDrawCmd& cmd : cmdList->CmdBuffer) {
            if (cmd.UserCallback) {
                cmd.UserCallback(cmdList, &cmd);
                idxOffset += cmd.ElemCount;
                continue;
            }

            ImVec2 clipMin = ImVec2(cmd.ClipRect.x - drawData->DisplayPos.x,
                                    cmd.ClipRect.y - drawData->DisplayPos.y);
            ImVec2 clipMax = ImVec2(cmd.ClipRect.z - drawData->DisplayPos.x,
                                    cmd.ClipRect.w - drawData->DisplayPos.y);
            if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y) {
                idxOffset += cmd.ElemCount;
                continue;
            }

            bgfx::setScissor(
                (uint16_t)clipMin.x, (uint16_t)clipMin.y,
                (uint16_t)(clipMax.x - clipMin.x), (uint16_t)(clipMax.y - clipMin.y)
            );

            bgfx::TextureHandle tex = GetRenderTexture(cmd.GetTexID());
            bgfx::setTexture(0, bd->uniformTexture, tex);
            bgfx::setVertexBuffer(0, &tvb, cmd.VtxOffset, numVtx);
            bgfx::setIndexBuffer(&tib, idxOffset, cmd.ElemCount);

            // ImGui icin dogru alpha blending: src_alpha / one_minus_src_alpha
            bgfx::setState(
                BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
            );

            bgfx::submit(bd->viewId, bd->program);
            idxOffset += cmd.ElemCount;
        }
    }
}
