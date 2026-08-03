// ImGui bgfx backend
// KLASIK YAKLASIM: RendererHasTextures kullanmadan, font atlas'i baslatmada
// RGBA32 formatinda tek seferde yukluyoruz. Bu Debug ve Release'de ayni sekilde calisir.

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
    bgfx::VertexLayout      layout;
    bgfx::ProgramHandle     program         = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle     uniformTexture  = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle     fontTexture     = BGFX_INVALID_HANDLE;
    uint8_t                 viewId          = 0;
};

static ImGui_ImplBgfx_Data* ImGui_ImplBgfx_GetBackendData() {
    return ImGui::GetCurrentContext()
        ? (ImGui_ImplBgfx_Data*)ImGui::GetIO().BackendRendererUserData
        : nullptr;
}

// Font atlas'i RGBA32 formatinda GPU'ya yukle
static bool CreateFontTexture() {
    ImGui_ImplBgfx_Data* bd = ImGui_ImplBgfx_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();

    unsigned char* pixels;
    int width, height;
    // RGBA32 formatinda atlas al - tum GPU'larda desteklenir, hic harf kaybetmez
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    const bgfx::Memory* mem = bgfx::copy(pixels, width * height * 4);

    bd->fontTexture = bgfx::createTexture2D(
        (uint16_t)width, (uint16_t)height, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        mem
    );

    if (!bgfx::isValid(bd->fontTexture)) {
        printf("[BgfxBackend] ERROR: Font texture creation failed!\n");
        return false;
    }

    printf("[BgfxBackend] Font texture created: %dx%d, RGBA8, handle.idx=%d\n",
        width, height, (int)bd->fontTexture.idx);

    // TexID olarak handle index'i set et
    io.Fonts->SetTexID((ImTextureID)(uintptr_t)bd->fontTexture.idx);

    return true;
}

bool ImGui_ImplBgfx_Init(uint8_t viewId) {
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "imgui_impl_bgfx";

    // SADECE VtxOffset destegi - RendererHasTextures YOK (lazy atlas istemiyoruz)
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    // NOT: ImGuiBackendFlags_RendererHasTextures kasıtlı olarak eklenmedi.
    // Bu flag, ImGui'nin glyphleri lazy load etmesine neden olur.
    // Lazy load + BGFX'in bir frame gecikmeli texture update sistemi,
    // bazi harflerin (W, F, K, O vb.) kaybolmasina neden oluyordu.

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
        return false;
    }

    printf("[BgfxBackend] Shader program created OK.\n");

    // Font atlas'i hemen olustur
    if (!CreateFontTexture()) {
        return false;
    }

    return true;
}

void ImGui_ImplBgfx_Shutdown() {
    ImGui_ImplBgfx_Data* bd = ImGui_ImplBgfx_GetBackendData();
    if (!bd) return;

    if (bgfx::isValid(bd->fontTexture))    bgfx::destroy(bd->fontTexture);
    if (bgfx::isValid(bd->uniformTexture)) bgfx::destroy(bd->uniformTexture);
    if (bgfx::isValid(bd->program))        bgfx::destroy(bd->program);

    delete bd;
    ImGui::GetIO().BackendRendererUserData = nullptr;
}

void ImGui_ImplBgfx_NewFrame() {
    // Klasik backend: her frame'de texture guncelleme yok.
    // Font atlas baslangicta bir kez yuklendi, degismez.
}

void ImGui_ImplBgfx_RenderDrawData(ImDrawData* drawData) {
    ImGui_ImplBgfx_Data* bd = ImGui_ImplBgfx_GetBackendData();
    if (!bd) return;

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

            // cmd.GetTexID() -> icon texture veya font texture handle'i
            bgfx::TextureHandle tex;
            tex.idx = (uint16_t)(uintptr_t)cmd.GetTexID();
            bgfx::setTexture(0, bd->uniformTexture, tex);
            bgfx::setVertexBuffer(0, &tvb, cmd.VtxOffset, numVtx);
            bgfx::setIndexBuffer(&tib, idxOffset, cmd.ElemCount);

            bgfx::setState(
                BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
            );

            bgfx::submit(bd->viewId, bd->program);
            idxOffset += cmd.ElemCount;
        }
    }
}
