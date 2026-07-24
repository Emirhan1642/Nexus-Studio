# Dinamik Mesh Çizimi ve GPU Skinning (Faz 5)

Bu aşamada önceden hardcoded olarak çizilen küp (cube) yapısından kurtulup dinamik skeletal mesh verisini çizen sisteme geçiş yapıldı.

## Neler Yapıldı?

1. **Vertex Layout & Data Structures (`Renderer.h`)**
   - Skinned meshler için içerisinde kemik ağırlıkları ve indekslerini taşıyan `SkinnedVertex` struct'ı ve layout yapısı bgfx tarafında tanımlandı.
   - İskelet destekli model (Mesh) çizimi için `MeshData` (Vertex Buffer ve Index Buffer saklayan) ve `MeshHandle` yapısı eklendi.

2. **Mesh Yükleme Sistemi (`Renderer.cpp`)**
   - AssetDatabase üzerinden `AssetGuid` alınarak dinamik Mesh'lerin yüklenmesini sağlayan `RendererSystem::getMeshHandle(guid)` fonksiyonu yazıldı. 
   - İskeletli `.fbx` verileri (ImportedSkeletalMesh), GPU'nun anlayacağı `SkinnedVertex` arraylerine çevrildi.

3. **GPU Skinning (Shader)**
   - `vs_pbr.sc` (Vertex Shader) güncellendi.
   - 64 adet 4x4 matris alan `u_boneTransforms` eklendi. Shader içerisinde bu matrisler, vertexlere aktarılan ağırlıklar ile harmanlanarak pozisyon ve normal vektörlerini animasyona uygun şekilde yeniden hesapladı.
   - Eğer gelen ağırlık 0 ise mesh, hiçbir iskelet kemiğinden etkilenmeyen bir **Static Mesh** gibi davrandı (tek shader üzerinden).

4. **Engine Entegrasyonu (`Part.cpp` & `Humanoid.cpp`)**
   - DataModel nesnesi olan `Part`, kendi içinde `currentBoneTransforms` tutacak şekilde güncellendi. Eğer `meshAssetGuid` tanımlıysa, ilgili Mesh'i çağırıp render Proxy'ye iletebilecek hale geldi.
   - `Humanoid::update` içindeki Animasyon hesaplamaları (`skeleton.computeWorldTransforms`) sonuçlarının RootPart'ın iskelet matrisleri içerisine geçirilmesi sağlandı (böylece Render Proxy, bir sonraki Frame'de bu değerleri Bgfx'e Uniform olarak gönderiyor).

## Test Adımları

- Editörü çalıştırdığınızda sürükle bırak yaptığınız iskelet modelleri artık statik küp yerine orijinal Mesh geometrisinde ve kendi kemik hiyerarşilerinde belirmelidir.
- (Eğer isterseniz) Karakterin `Humanoid` kontrolcüsünü çalıştırdığınızda (yürüme gibi) animasyon verileri doğrudan iskeleti tetikleyecek, karakter hareket edecektir.
