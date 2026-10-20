#include "pch.h"
#include "EditorApi.h"

extern "C" __declspec(dllexport) void __stdcall ShowLayrixEditor() {
    RequestShowEditor();
}

extern "C" __declspec(dllexport) void __stdcall ShowLayrixSettings() {
    RequestShowEditor(true);
}
