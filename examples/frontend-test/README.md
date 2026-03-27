# Frontend Test Example

TitanKV'yi hızlıca test etmek için minimal bir frontend + local API örneği.

## Çalıştırma

Repo kökünden:

```bash
node examples/frontend-test/server.js
```

Tarayıcıda aç:

```text
http://127.0.0.1:3030
```

## Ne Test Edebilirsin?

- `Put`: key/value yazma (+ opsiyonel TTL)
- `Get`: key okuma
- `Delete`: key silme
- `Keys`: prefix ile listeleme

## Not

- Veriler `examples/frontend-test/data` altında tutulur.
- Bu örnek sadece local test içindir.
