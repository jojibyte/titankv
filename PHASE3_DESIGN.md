# TitanKV Phase 3 - Next Generation Architecture (v3.x)

Bu doküman, TitanKV'nin "Devler Ligi" vizyonunu gerçekleştirmek için teknik yol haritasını içerir.

## Mimari Hedefler

### 1. Asenkron Disk I/O (Async N-API)
Mevcut durumda `put()`, `get()`, ve WAL (Write-Ahead-Log) işlemleri C++ üzerinde Node.js'in ana event loop'unu hafif de olsa bloke ederek (senkron) çalışmaktadır. 
Milyonlarca isteği işleyebilmek için:
- **Napi::AsyncWorker** veya `uv_async_send` kullanılarak C++ arka plan thread pool'ları devreye alınmalı.
- JavaScript API'si Promises (veya callback) dönecek şekilde genişletilmeli (`await db.putAsync('key', 'value')`).
- Senkron API (örn: `db.put()`) eski node.js kod tabanları için korunurken (geriye dönük uyumluluk), varsayılan odak asenkrona kaymalı.

### 2. LSM-Tree & Disk Sızdırması (Spill to Disk)
Veritabanı şu anda tüm anahtarları RAM'de tutmakta. Bu hız açısından muazzam olsa da RAM kapasitesi sınırlıdır.
- Bellekteki (MemTable) veriler belirli bir boyuta (`maxMemoryBytes`) ulaştığında Zstd kullanılarak sıkıştırılıp "SST (Sorted String Table)" formatında diske sızdırılacak.
- Bloom Filter'lar: Diskteki SST dosyalarından veri okurken okuma işlemlerini hızlandırmak ve gereksiz Disk kilitlenmelerinden kaçınmak için kullanılacak.
- Arka planda SSD sıkıştırma ve birleştirme (Compaction) thread'leri olacak.

## Sonraki Adımlar
1. `src/binding/addon.cpp` güncellenerek, test amaçlı `putAsync` gibi deneysel bir Asenkron metod eklenecek.
2. WAL işlemleri I/O engelini aşması için thread'lere dağıtılacak.
3. RAM verisi SST'ye dökülecek şekilde `src/core/storage.hpp` yapısı genişletilecek.