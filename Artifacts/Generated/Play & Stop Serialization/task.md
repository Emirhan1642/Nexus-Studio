# Task: Play/Stop Durum Serileştirmesi (Serialization)

- `[x]` **Görev 1: DataModelSerializer Başlık Dosyası**
  - `[x]` `Engine/Core/DataModel/DataModelSerializer.h` oluşturulacak.
  - `[x]` `serialize` ve `deserialize` statik fonksiyonları tanımlanacak.
  
- `[x]` **Görev 2: DataModelSerializer Kaynak Dosyası**
  - `[x]` `Engine/Core/DataModel/DataModelSerializer.cpp` oluşturulacak.
  - `[x]` `TypeRegistry` üzerinden properties okunarak JSON'a yazılması sağlanacak.
  - `[x]` Float, Int, Bool, std::string, Engine::Math::Vector3 tipleri için destek eklenecek.
  - `[x]` Çocuk nesnelerin (children) rekürsif olarak işlenmesi yapılacak.
  
- `[x]` **Görev 3: CMakeLists.txt Güncellemesi**
  - `[x]` `Engine/Core/CMakeLists.txt` dosyasına `DataModelSerializer.cpp` eklenecek.
  - `[x]` `nlohmann_json` bağımlılığı projeye dâhil edilecek (eğer edilmediyse).
  
- `[x]` **Görev 4: Editör (Play/Stop) Entegrasyonu**
  - `[x]` `Editor/Main.cpp` içindeki Play/Stop blokları `DataModelSerializer` kullanacak şekilde güncellenecek.
  - `[x]` Eski `DataModelSnapshot.h` include'u kaldırılacak.
  
- `[x]` **Görev 5: Derleme ve Test**
  - `[x]` CMake üzerinden proje derlenecek.
  - `[x]` Play moduna geçildiğinde objelerin JSON olarak kaydedildiği ve Stop ile geri yüklendiği doğrulanacak.
