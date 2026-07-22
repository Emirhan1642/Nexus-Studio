# Aşama 1 Tamamlandı (Faz 0 & Faz 1)

Oyun motorunun **Faz 0** (İskelet) ve **Faz 1** (Reflection ve DataModel) geliştirmeleri için gerekli olan tüm temel C++ dosyalarını, CMake konfigürasyonlarını ve birim testlerini (GoogleTest ile) oluşturdum. 

## Neler Yapıldı?

1. **Proje İskeleti (CMake):** 
   - Projenin `ThirdParty`, `Engine`, `Editor` ve `Tests` olarak modüler bir şekilde derlenebilmesi için `CMakeLists.txt` dosyaları oluşturuldu.
   - `ThirdParty` klasöründe **bgfx** (render motoru), **glfw** (pencere yönetimi) ve **googletest** (testler için) projeye dahil edildi.
2. **Reflection Sistemi (Core):**
   - Sınıfları çalışma zamanında (runtime) tanıtmak için `TypeRegistry` tasarlandı.
   - Kalıtım sorununu (Static Initialization Order Fiasco) çözen `deferBaseClass` ve `finalize` mantığı eklendi.
   - Enum'lar için `EnumRegistry` yazıldı.
   - Geliştiricinin sınıflarını çok kolay tanıtabilmesi için `ClassBuilder` (Fluent API) inşa edildi.
3. **DataModel Sistemi (Core):**
   - Roblox tarzı Instance hiyerarşisinin temeli olan `Instance` sınıfı oluşturuldu. Hafıza yönetimi için `weak_ptr` (parent) ve `shared_ptr` (children) kullanıldı.
   - `DataModel` (kök nesne) ve `Part` (temel 3D obje) sınıfları yazıldı ve reflection sistemine kayıtları gerçekleştirildi.
4. **Boş Pencere (Editor):**
   - Editor executable'ı için `Main.cpp` dosyasına GLFW ve bgfx başlatma kodları eklendi. TypeRegistry başarılı şekilde başlatılıp boş bir pencere açılması sağlandı.
5. **Birim Testleri (Tests):**
   - Yazılan reflection sisteminin, hiyerarşi oluşturmanın ve kalıtım (`IsA`) mantığının doğru çalıştığını doğrulamak için `ReflectionTests.cpp` yazıldı.

## Sonuç ve Çalıştırma

Terminalden (CMake aracılığıyla) projeyi başarıyla derledik ve **tüm birim testlerini sorunsuzca geçtik!** (6/6 test başarılı). 

Projenin Editor'ünü başlatıp boş ekrana ulaşmak için terminalinizden şu komutu çalıştırabilirsiniz:
```powershell
.\build\bin\Debug\NexusStudioEditor.exe
```

Testleri dilediğiniz zaman tekrar çalıştırmak isterseniz:
```powershell
.\build\bin\Debug\NexusStudioTests.exe
```
