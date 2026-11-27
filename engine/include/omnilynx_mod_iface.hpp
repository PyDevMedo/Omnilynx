#pragma once

#include "wendo_mod.h"
#include <memory>

namespace omnilynx {

// Abstract standalone mod interface for decoupled, RAII-safe C++ mods
class IMod {
public:
    virtual ~IMod() = default;
    virtual const wchar_t* Id() const noexcept = 0;
    virtual const wchar_t* Name() const noexcept = 0;
    virtual const wchar_t* Version() const noexcept = 0;
    virtual bool OnInit() = 0;
    virtual void OnUninit() noexcept = 0;
    virtual void OnSettingsChanged() {}
};

// Bridge a concrete IMod implementation to the Omnilynx DLL lifecycle
template <typename ModT>
class ModHost {
public:
    static BOOL Init() {
        if (!s_instance) {
            s_instance = std::make_unique<ModT>();
        }
        return s_instance->OnInit() ? TRUE : FALSE;
    }

    static void Uninit() noexcept {
        if (s_instance) {
            s_instance->OnUninit();
            s_instance.reset();
        }
    }

    static void SettingsChanged() {
        if (s_instance) {
            s_instance->OnSettingsChanged();
        }
    }

private:
    static inline std::unique_ptr<ModT> s_instance;
};

} // namespace omnilynx

#define OMNILYNX_REGISTER_MOD(ModClass)              \
    BOOL Wh_ModInit() {                              \
        return omnilynx::ModHost<ModClass>::Init();  \
    }                                                \
    void Wh_ModUninit() {                            \
        omnilynx::ModHost<ModClass>::Uninit();       \
    }                                                \
    void Wh_ModSettingsChanged() {                   \
        omnilynx::ModHost<ModClass>::SettingsChanged(); \
    }
