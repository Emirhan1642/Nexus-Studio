#pragma once
#include <GLFW/glfw3.h>

// ─────────────────────────────────────────────────────────────────────────
// RedrawScheduler (v2)
//
// ONEMLI DUZELTME: Ilk versiyon, Idle modda glfwWaitEventsTimeout ile
// present araligini (~20 FPS'e kadar) uzatiyordu. Bu, adaptive-sync
// (G-Sync/FreeSync) monitorlerde flicker'i AZALTMAK yerine ARTIRDI:
// uygulama Idle<->Active arasinda ~20Hz <-> ~144Hz gibi cok buyuk ve ani
// bir refresh-hizi sicramasi yapiyordu. 20Hz cogu VRR monitorun "VRR
// floor"unun (genelde 40-48Hz) ALTINDA kaldigi icin monitorun Low
// Framerate Compensation (LFC) mekanizmasini tetikliyor; LFC frame'leri
// katlayarak gosteriyor, bu da gamma/parlaklik pulsationu (soluklasma/
// canlanma) olarak goruluyor.
//
// Godot'nun resmi VRR-flicker dokumantasyonundaki cozum de bunu dogruluyor:
// sorun olustugunda onerilen aksiyon "Update Continuously"yi ACMAK, yani
// present kadansini SABIT/native tutmak -- dusuk FPS'e throttle etmek degil.
//
// Bu yuzden v2'de present kadansi ARTIK HIC DEGISTIRILMIYOR (her zaman
// glfwPollEvents, asla glfwWaitEventsTimeout). Idle/Active/Playing ayrimi
// sadece ISTEGE BAGLI agir CPU islerinin (thumbnail polling, gizmo
// hit-test, dosya sistemi taramasi vb. -- ekranda GORUNMEYEN/kritik
// olmayan isler) throttle edilmesi icin kullanilir. Present/refresh
// hizina ASLA dokunulmaz.
//
// Detay: 18_RedrawScheduler_Idle_Throttle_Frame_Pacing.md (v2 notuyla
// birlikte okunmali -- orijinal dokumandaki "present'i throttle et"
// onerisi VRR monitorler icin GECERSIZ, bu dosyadaki not gecerli olan.)
// ─────────────────────────────────────────────────────────────────────────

enum class RedrawMode {
    Idle,     // Girdi/dirty yok, pencere odaksiz VEYA grace suresi doldu.
              // SADECE opsiyonel agir islerin throttle edilmesi icin isaret;
              // present kadansini etkilemez.
    Active,   // Aktif girdi VEYA bekleyen dirty flag var.
    Playing   // Play/Simulate modu -- agir is throttle'i da devre disi.
};

class RedrawScheduler {
public:
    static RedrawScheduler& instance() {
        static RedrawScheduler s_instance;
        return s_instance;
    }

    // Main.cpp loop basinda cagrilir. Present kadansini DEGISTIRMEZ (her
    // zaman glfwPollEvents); sadece bu turun RedrawMode'unu gunceller.
    void pumpEvents(GLFWwindow* window);

    // Herhangi bir sistem (PropertyDescriptor setter'i, efsw dosya izleyici,
    // network paket alimi vb.) ekranda bir seyin degistigini bildirmek
    // icin bunu cagirir.
    void requestRedraw();

    // Play/Stop toggle (F5) burada bildirilir.
    void setPlaying(bool playing) { m_isPlaying = playing; }
    bool isPlaying() const { return m_isPlaying; }

    RedrawMode currentMode() const { return m_currentMode; }

    // Idle modda, ekrana dogrudan yansimayan agir/opsiyonel islerin
    // (thumbnail polling, gizmo raycast, periyodik dosya taramasi vb.)
    // ne siklikta calisacagini sinirlar. Present hizini ETKILEMEZ --
    // sadece CPU is yukunu azaltmak icindir. Ornek kullanim:
    //
    //   if (RedrawScheduler::instance().shouldRunHeavyIdleWork(now))
    //       thumbnailCache.pollPending();
    //
    bool shouldRunHeavyIdleWork(double now);

    void setIdleGraceSeconds(float s)          { m_idleGraceSeconds = s; }
    void setHeavyWorkIntervalSeconds(float s)  { m_heavyWorkInterval = s; }

private:
    RedrawScheduler() = default;
    RedrawMode resolveMode(double now, bool windowFocused);

    bool   m_isPlaying          = false;
    bool   m_dirty              = true;   // baslangicta true: ilk frame mutlaka "Active" sayilir
    double m_lastActivityTime   = 0.0;    // son input/dirty zaman damgasi (glfwGetTime())
    float  m_idleGraceSeconds   = 0.4f;   // son aktiviteden sonra Idle'a gecis gecikmesi
    float  m_heavyWorkInterval  = 0.1f;   // Idle'da agir isler en fazla ~10Hz'de calisir
    double m_lastHeavyWorkTime  = 0.0;
    RedrawMode m_currentMode    = RedrawMode::Active;
};