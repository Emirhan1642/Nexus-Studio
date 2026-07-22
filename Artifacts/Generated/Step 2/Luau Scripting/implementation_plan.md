# Aşama 3: Luau Scripting Entegrasyonu (Faz 3)

Bu aşamada Roblox'un da kullandığı Luau motorunu projemize entegre edip, daha önce geliştirdiğimiz Reflection (Faz 1) sistemimiz ile bağlıyoruz. Bu sayede yazılan script'ler motor içerisindeki DataModel nesnelerine doğrudan ve generic (sınıfa özel kod yazmadan) bir şekilde erişebilecek.

## User Review Required

> [!IMPORTANT]
> - Luau entegrasyonu için `FetchContent` kullanılarak `luau-lang/luau` deposu eklenecektir. Bu indirme ve derleme işlemine projenin derleme süresini biraz uzatabilir.
> - "Script Timeout" (Bölüm 10'da belirtilen sonsuz döngü koruması) temel olarak bu faza dahil edilmelidir. Faz 3 içerisinde Luau'nun instruction limiti (interrupt callback) mekanizmasını kullanarak basit bir timeout koruması ekleyeceğim.

## Proposed Changes

### 1. Bağımlılıkların Eklenmesi
Luau kütüphanesi CMake üzerinden indirilecek ve projeye bağlanacak.

#### [MODIFY] [ThirdParty/CMakeLists.txt](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus Studio/ThirdParty/CMakeLists.txt)
- `luau` reposunu `FetchContent` ile dahil etmek. `Luau.VM`, `Luau.Compiler`, `Luau.Ast` modüllerine erişim sağlayacağız.

### 2. Signal / Event Sistemi (Engine/Core)
Roblox'taki gibi nesneler arası haberleşmeyi ve event tetiklemeyi (`part.Touched:Connect(...)`) sağlamak için genel bir Signal sınıfı.

#### [NEW] [Engine/Core/Signal.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus Studio/Engine/Core/Signal.h)
- Parametre olarak `std::any` (veya `std::vector<std::any>`) alan, çoklu dinleyici destekleyen Signal altyapısı.

### 3. Luau Scripting Modülü (Engine/Scripting)
Script'lerin çalıştırılması, duraklatılması (coroutine/wait) ve izole (sandboxing) edilmesi.

#### [NEW] [Engine/Scripting/CMakeLists.txt](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus Studio/Engine/Scripting/CMakeLists.txt)
- Yeni alt modül tanımı.
#### [NEW] [Engine/Scripting/LuauRuntime/LuauVM.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus Studio/Engine/Scripting/LuauRuntime/LuauVM.h) & .cpp
- `LuauVM` singleton. Güvensiz kütüphanelerin silinmesi (sandboxing) ve Script'lere izole global ortam verilmesi.
#### [NEW] [Engine/Scripting/LuauRuntime/ScriptScheduler.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus Studio/Engine/Scripting/LuauRuntime/ScriptScheduler.h) & .cpp
- Kooperatif multitasking: Script'lerin `wait()` metodunu çağırdığında coroutine'in askıya alınması ve motoru dondurmadan süre dolduğunda devam etmesi.

### 4. Reflection Köprüsü (Bindings)
C++ nesneleriyle Luau arasındaki Generic köprü.

#### [NEW] [Engine/Scripting/LuauRuntime/InstanceBinding.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus Studio/Engine/Scripting/LuauRuntime/InstanceBinding.h) & .cpp
- `__index` ve `__newindex` metametodları. `TypeRegistry` kullanılarak C++ property'lerinin dinamik olarak okunup/yazılması.
#### [NEW] [Engine/Scripting/LuauRuntime/Vector3Binding.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus Studio/Engine/Scripting/LuauRuntime/Vector3Binding.h) & .cpp
- Değer tipi (value type) olan Vector3 için ayrıntılı binding ve matematiksel operatörlerin (`+`, `-`, `*`) aktarılması.

### 5. Script Sınıfı (DataModel)
Script'leri sahnede bir öğe olarak tutmak için.

#### [NEW] [Engine/Core/DataModel/Script.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus Studio/Engine/Core/DataModel/Script.h) & .cpp
- `Instance` sınıfından kalıtım alan, `source` property'sine sahip olan ve `onAddedToWorkspace` olduğunda kodu LuauVM üzerinden çalıştıran `Script` sınıfı.

## Verification Plan

### Manual Verification
1. `NexusStudioEditor` içine yeni bir `Script` nesnesi eklenecek ve `Workspace`'e dahil edilecek.
2. Script içerisinde `wait(1.0)` fonksiyonu kullanılarak coroutine mekanizmasının çalıştığı, motorun donmadığı gözlemlenecek.
3. Script üzerinden sahnede var olan bir küpün (`Part`) pozisyonu (`Position`) her karede değiştirilerek Luau ↔ Reflection ↔ Renderer köprüsünün tam birleştiği doğrulanacak (küp ekranda hareket edecek).
4. Tehlikeli bir fonksiyon (örneğin `os.execute`) kullanıldığında motorun bunu engellediği teyit edilecek.
