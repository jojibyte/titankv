#include <napi.h>
#include "titankv.hpp"
#include <memory>
#include <vector>

class TitanKV : public Napi::ObjectWrap<TitanKV> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    TitanKV(const Napi::CallbackInfo& info);

private:
    std::unique_ptr<titan::TitanEngine> engine_;

    Napi::Value Put(const Napi::CallbackInfo& info);
    Napi::Value PutAsync(const Napi::CallbackInfo& info);
    Napi::Value Get(const Napi::CallbackInfo& info);
    Napi::Value GetAsync(const Napi::CallbackInfo& info);
    Napi::Value Del(const Napi::CallbackInfo& info);
    Napi::Value Has(const Napi::CallbackInfo& info);
    Napi::Value Size(const Napi::CallbackInfo& info);
    Napi::Value Clear(const Napi::CallbackInfo& info);
    Napi::Value Incr(const Napi::CallbackInfo& info);
    Napi::Value Decr(const Napi::CallbackInfo& info);
    Napi::Value Keys(const Napi::CallbackInfo& info);
    Napi::Value KeysAsync(const Napi::CallbackInfo& info);
    Napi::Value Scan(const Napi::CallbackInfo& info);
    Napi::Value ScanAsync(const Napi::CallbackInfo& info);
    Napi::Value Range(const Napi::CallbackInfo& info);
    Napi::Value RangeAsync(const Napi::CallbackInfo& info);
    Napi::Value CountPrefix(const Napi::CallbackInfo& info);
    Napi::Value CountPrefixAsync(const Napi::CallbackInfo& info);
    Napi::Value PutBatch(const Napi::CallbackInfo& info);
    Napi::Value GetBatch(const Napi::CallbackInfo& info);
    Napi::Value PutBatchAsync(const Napi::CallbackInfo& info);
    Napi::Value GetBatchAsync(const Napi::CallbackInfo& info);
    Napi::Value Flush(const Napi::CallbackInfo& info);
    Napi::Value FlushAsync(const Napi::CallbackInfo& info);
    Napi::Value Compact(const Napi::CallbackInfo& info);
    Napi::Value CompactAsync(const Napi::CallbackInfo& info);
    Napi::Value Close(const Napi::CallbackInfo& info);
    Napi::Value GetStats(const Napi::CallbackInfo& info);
};

Napi::Object TitanKV::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "TitanKV", {
        InstanceMethod("put", &TitanKV::Put),
        InstanceMethod("putAsync", &TitanKV::PutAsync),
        InstanceMethod("get", &TitanKV::Get),
        InstanceMethod("getAsync", &TitanKV::GetAsync),
        InstanceMethod("del", &TitanKV::Del),
        InstanceMethod("has", &TitanKV::Has),
        InstanceMethod("size", &TitanKV::Size),
        InstanceMethod("clear", &TitanKV::Clear),
        InstanceMethod("incr", &TitanKV::Incr),
        InstanceMethod("decr", &TitanKV::Decr),
        InstanceMethod("keys", &TitanKV::Keys),
        InstanceMethod("keysAsync", &TitanKV::KeysAsync),
        InstanceMethod("scan", &TitanKV::Scan),
        InstanceMethod("scanAsync", &TitanKV::ScanAsync),
        InstanceMethod("range", &TitanKV::Range),
        InstanceMethod("rangeAsync", &TitanKV::RangeAsync),
        InstanceMethod("countPrefix", &TitanKV::CountPrefix),
        InstanceMethod("countPrefixAsync", &TitanKV::CountPrefixAsync),
        InstanceMethod("putBatch", &TitanKV::PutBatch),
        InstanceMethod("getBatch", &TitanKV::GetBatch),
        InstanceMethod("putBatchAsync", &TitanKV::PutBatchAsync),
        InstanceMethod("getBatchAsync", &TitanKV::GetBatchAsync),
        InstanceMethod("flush", &TitanKV::Flush),
        InstanceMethod("flushAsync", &TitanKV::FlushAsync),
        InstanceMethod("compact", &TitanKV::Compact),
        InstanceMethod("compactAsync", &TitanKV::CompactAsync),
        InstanceMethod("close", &TitanKV::Close),
        InstanceMethod("stats", &TitanKV::GetStats),
    });

    Napi::FunctionReference* constructor = new Napi::FunctionReference();
    *constructor = Napi::Persistent(func);
    env.SetInstanceData(constructor);

    exports.Set("TitanKV", func);
    return exports;
}

TitanKV::TitanKV(const Napi::CallbackInfo& info) : Napi::ObjectWrap<TitanKV>(info) {
    Napi::Env env = info.Env();
    std::string path = "";
    int compression_level = 3;
    size_t max_memory_bytes = 0;
    titan::RecoveryMode recovery_mode = titan::RecoveryMode::Permissive;
    bool bloom_filter_enabled = true;
    bool auto_compact_enabled = false;
    size_t compact_min_ops = 2000;
    double compact_tombstone_ratio = 0.35;
    size_t compact_min_wal_bytes = 4 * 1024 * 1024;

    if (info.Length() > 0 && info[0].IsString()) {
        path = info[0].As<Napi::String>().Utf8Value();
    }

    if (info.Length() > 1 && info[1].IsObject()) {
        Napi::Object opts = info[1].As<Napi::Object>();
        if (opts.Has("compressionLevel")) {
            compression_level = opts.Get("compressionLevel").As<Napi::Number>().Int32Value();
        }
        if (opts.Has("maxMemoryBytes")) {
            max_memory_bytes = static_cast<size_t>(opts.Get("maxMemoryBytes").As<Napi::Number>().Int64Value());
        }
        if (opts.Has("recoverMode") && opts.Get("recoverMode").IsString()) {
            const std::string mode = opts.Get("recoverMode").As<Napi::String>().Utf8Value();
            if (mode == "strict") {
                recovery_mode = titan::RecoveryMode::Strict;
            }
        }
        if (opts.Has("bloomFilter") && opts.Get("bloomFilter").IsBoolean()) {
            bloom_filter_enabled = opts.Get("bloomFilter").As<Napi::Boolean>().Value();
        }
        if (opts.Has("autoCompact") && opts.Get("autoCompact").IsBoolean()) {
            auto_compact_enabled = opts.Get("autoCompact").As<Napi::Boolean>().Value();
        }
        if (opts.Has("compactMinOps") && opts.Get("compactMinOps").IsNumber()) {
            compact_min_ops = static_cast<size_t>(opts.Get("compactMinOps").As<Napi::Number>().Int64Value());
        }
        if (opts.Has("compactTombstoneRatio") && opts.Get("compactTombstoneRatio").IsNumber()) {
            compact_tombstone_ratio = opts.Get("compactTombstoneRatio").As<Napi::Number>().DoubleValue();
        }
        if (opts.Has("compactMinWalBytes") && opts.Get("compactMinWalBytes").IsNumber()) {
            compact_min_wal_bytes = static_cast<size_t>(opts.Get("compactMinWalBytes").As<Napi::Number>().Int64Value());
        }
    }

    try {
        engine_ = std::make_unique<titan::TitanEngine>(path, recovery_mode, bloom_filter_enabled);
        engine_->setCompressionLevel(compression_level);
        engine_->setCompactionPolicy(compact_min_ops, compact_tombstone_ratio, compact_min_wal_bytes);
        engine_->setAutoCompactEnabled(auto_compact_enabled);
        if (max_memory_bytes > 0) {
            engine_->setMaxMemoryBytes(max_memory_bytes);
        }
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
    }
}

Napi::Value TitanKV::Put(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2) {
        Napi::TypeError::New(env, "Expected key and value").ThrowAsJavaScriptException();
        return env.Null();
    }
    std::string key = info[0].As<Napi::String>().Utf8Value();
    std::string value = info[1].As<Napi::String>().Utf8Value();
    int64_t ttl = 0;
    if (info.Length() > 2 && info[2].IsNumber()) {
        ttl = info[2].As<Napi::Number>().Int64Value();
    }
    try {
        engine_->put(key, value, ttl);
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
    }
    return env.Undefined();
}

class PutAsyncWorker : public Napi::AsyncWorker {
public:
    PutAsyncWorker(Napi::Env& env, titan::TitanEngine* engine, std::string key, std::string value, int64_t ttl)
        : Napi::AsyncWorker(env), deferred(Napi::Promise::Deferred::New(env)), engine_(engine), key_(std::move(key)), value_(std::move(value)), ttl_(ttl) {}

    ~PutAsyncWorker() {}

    void Execute() override {
        try {
            engine_->put(key_, value_, ttl_);
        } catch (const std::exception& e) {
            SetError(e.what());
        }
    }

    void OnOK() override {
        deferred.Resolve(Env().Undefined());
    }

    void OnError(const Napi::Error& e) override {
        deferred.Reject(e.Value());
    }

    Napi::Promise GetPromise() {
        return deferred.Promise();
    }

private:
    Napi::Promise::Deferred deferred;
    titan::TitanEngine* engine_;
    std::string key_;
    std::string value_;
    int64_t ttl_;
};

Napi::Value TitanKV::PutAsync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2) {
        Napi::TypeError::New(env, "Expected key and value").ThrowAsJavaScriptException();
        return env.Null();
    }
    std::string key = info[0].As<Napi::String>().Utf8Value();
    std::string value = info[1].As<Napi::String>().Utf8Value();
    int64_t ttl = 0;
    if (info.Length() > 2 && info[2].IsNumber()) {
        ttl = info[2].As<Napi::Number>().Int64Value();
    }

    PutAsyncWorker* worker = new PutAsyncWorker(env, engine_.get(), key, value, ttl);
    worker->Queue();
    return worker->GetPromise();
}

Napi::Value TitanKV::Get(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return env.Null();
    try {
        auto result = engine_->get(info[0].As<Napi::String>().Utf8Value());
        return result ? Napi::String::New(env, *result) : env.Null();
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

class GetAsyncWorker : public Napi::AsyncWorker {
public:
    GetAsyncWorker(Napi::Env& env, titan::TitanEngine* engine, std::string key)
        : Napi::AsyncWorker(env), deferred(Napi::Promise::Deferred::New(env)), engine_(engine), key_(std::move(key)) {}

    ~GetAsyncWorker() {}

    void Execute() override {
        try {
            result_ = engine_->get(key_);
        } catch (const std::exception& e) {
            SetError(e.what());
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        if (result_) {
            deferred.Resolve(Napi::String::New(env, *result_));
        } else {
            deferred.Resolve(env.Null());
        }
    }

    void OnError(const Napi::Error& e) override {
        deferred.Reject(e.Value());
    }

    Napi::Promise GetPromise() {
        return deferred.Promise();
    }

private:
    Napi::Promise::Deferred deferred;
    titan::TitanEngine* engine_;
    std::string key_;
    std::optional<std::string> result_;
};

Napi::Value TitanKV::GetAsync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) {
        Napi::TypeError::New(env, "Expected key").ThrowAsJavaScriptException();
        return env.Null();
    }
    std::string key = info[0].As<Napi::String>().Utf8Value();

    GetAsyncWorker* worker = new GetAsyncWorker(env, engine_.get(), key);
    worker->Queue();
    return worker->GetPromise();
}

Napi::Value TitanKV::Del(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return Napi::Boolean::New(env, false);
    try {
        return Napi::Boolean::New(env, engine_->del(info[0].As<Napi::String>().Utf8Value()));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }
}

Napi::Value TitanKV::Has(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return Napi::Boolean::New(env, false);
    try {
        return Napi::Boolean::New(env, engine_->has(info[0].As<Napi::String>().Utf8Value()));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }
}

Napi::Value TitanKV::Size(const Napi::CallbackInfo& info) {
    try {
        return Napi::Number::New(info.Env(), static_cast<double>(engine_->size()));
    } catch (const std::exception& e) {
        Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
        return info.Env().Null();
    }
}

Napi::Value TitanKV::Clear(const Napi::CallbackInfo& info) {
    try {
        engine_->clear();
    } catch (const std::exception& e) {
        Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
    }
    return info.Env().Undefined();
}

Napi::Value TitanKV::Incr(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    int64_t delta = 1;
    if (info.Length() > 1 && info[1].IsNumber()) delta = info[1].As<Napi::Number>().Int64Value();
    try {
        return Napi::Number::New(env, static_cast<double>(engine_->incr(info[0].As<Napi::String>().Utf8Value(), delta)));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value TitanKV::Decr(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    int64_t delta = 1;
    if (info.Length() > 1 && info[1].IsNumber()) delta = info[1].As<Napi::Number>().Int64Value();
    try {
        return Napi::Number::New(env, static_cast<double>(engine_->decr(info[0].As<Napi::String>().Utf8Value(), delta)));
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value TitanKV::Keys(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    size_t limit = 1000;
    if (info.Length() > 0 && info[0].IsNumber()) limit = info[0].As<Napi::Number>().Int64Value();

    try {
        auto keys = engine_->keys(limit);
        Napi::Array arr = Napi::Array::New(env, keys.size());
        for (size_t i = 0; i < keys.size(); i++) {
            arr.Set(i, Napi::String::New(env, keys[i]));
        }
        return arr;
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

class KeysAsyncWorker : public Napi::AsyncWorker {
public:
    KeysAsyncWorker(Napi::Env& env, titan::TitanEngine* engine, size_t limit)
        : Napi::AsyncWorker(env), deferred(Napi::Promise::Deferred::New(env)), engine_(engine), limit_(limit) {}

    ~KeysAsyncWorker() {}

    void Execute() override {
        try {
            keys_ = engine_->keys(limit_);
        } catch (const std::exception& e) {
            SetError(e.what());
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        Napi::Array arr = Napi::Array::New(env, keys_.size());
        for (size_t i = 0; i < keys_.size(); i++) {
            arr.Set(i, Napi::String::New(env, keys_[i]));
        }
        deferred.Resolve(arr);
    }

    void OnError(const Napi::Error& e) override {
        deferred.Reject(e.Value());
    }

    Napi::Promise GetPromise() {
        return deferred.Promise();
    }

private:
    Napi::Promise::Deferred deferred;
    titan::TitanEngine* engine_;
    size_t limit_;
    std::vector<std::string> keys_;
};

Napi::Value TitanKV::KeysAsync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    size_t limit = 1000;
    if (info.Length() > 0 && info[0].IsNumber()) {
        limit = info[0].As<Napi::Number>().Int64Value();
    }

    KeysAsyncWorker* worker = new KeysAsyncWorker(env, engine_.get(), limit);
    worker->Queue();
    return worker->GetPromise();
}

Napi::Value TitanKV::Scan(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return Napi::Array::New(env, 0);
    std::string prefix = info[0].As<Napi::String>().Utf8Value();
    size_t limit = 1000;
    if (info.Length() > 1 && info[1].IsNumber()) limit = info[1].As<Napi::Number>().Int64Value();

    try {
        auto pairs = engine_->scan(prefix, limit);
        Napi::Array arr = Napi::Array::New(env, pairs.size());
        for (size_t i = 0; i < pairs.size(); i++) {
            Napi::Array pair = Napi::Array::New(env, 2);
            pair.Set((uint32_t)0, Napi::String::New(env, pairs[i].first));
            pair.Set((uint32_t)1, Napi::String::New(env, pairs[i].second));
            arr.Set(i, pair);
        }
        return arr;
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

class ScanAsyncWorker : public Napi::AsyncWorker {
public:
    ScanAsyncWorker(Napi::Env& env, titan::TitanEngine* engine, std::string prefix, size_t limit)
        : Napi::AsyncWorker(env),
          deferred(Napi::Promise::Deferred::New(env)),
          engine_(engine),
          prefix_(std::move(prefix)),
          limit_(limit) {}

    ~ScanAsyncWorker() {}

    void Execute() override {
        try {
            pairs_ = engine_->scan(prefix_, limit_);
        } catch (const std::exception& e) {
            SetError(e.what());
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        Napi::Array arr = Napi::Array::New(env, pairs_.size());
        for (size_t i = 0; i < pairs_.size(); i++) {
            Napi::Array pair = Napi::Array::New(env, 2);
            pair.Set((uint32_t)0, Napi::String::New(env, pairs_[i].first));
            pair.Set((uint32_t)1, Napi::String::New(env, pairs_[i].second));
            arr.Set(i, pair);
        }
        deferred.Resolve(arr);
    }

    void OnError(const Napi::Error& e) override {
        deferred.Reject(e.Value());
    }

    Napi::Promise GetPromise() {
        return deferred.Promise();
    }

private:
    Napi::Promise::Deferred deferred;
    titan::TitanEngine* engine_;
    std::string prefix_;
    size_t limit_;
    std::vector<std::pair<std::string, std::string>> pairs_;
};

Napi::Value TitanKV::ScanAsync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) {
        Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
        deferred.Resolve(Napi::Array::New(env, 0));
        return deferred.Promise();
    }

    std::string prefix = info[0].As<Napi::String>().Utf8Value();
    size_t limit = 1000;
    if (info.Length() > 1 && info[1].IsNumber()) {
        limit = info[1].As<Napi::Number>().Int64Value();
    }

    ScanAsyncWorker* worker = new ScanAsyncWorker(env, engine_.get(), std::move(prefix), limit);
    worker->Queue();
    return worker->GetPromise();
}

Napi::Value TitanKV::Range(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2) return Napi::Array::New(env, 0);
    std::string start = info[0].As<Napi::String>().Utf8Value();
    std::string end = info[1].As<Napi::String>().Utf8Value();
    size_t limit = 1000;
    if (info.Length() > 2 && info[2].IsNumber()) limit = info[2].As<Napi::Number>().Int64Value();

    try {
        auto pairs = engine_->range(start, end, limit);
        Napi::Array arr = Napi::Array::New(env, pairs.size());
        for (size_t i = 0; i < pairs.size(); i++) {
            Napi::Array pair = Napi::Array::New(env, 2);
            pair.Set((uint32_t)0, Napi::String::New(env, pairs[i].first));
            pair.Set((uint32_t)1, Napi::String::New(env, pairs[i].second));
            arr.Set(i, pair);
        }
        return arr;
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

class RangeAsyncWorker : public Napi::AsyncWorker {
public:
    RangeAsyncWorker(Napi::Env& env, titan::TitanEngine* engine, std::string start, std::string end, size_t limit)
        : Napi::AsyncWorker(env),
          deferred(Napi::Promise::Deferred::New(env)),
          engine_(engine),
          start_(std::move(start)),
          end_(std::move(end)),
          limit_(limit) {}

    ~RangeAsyncWorker() {}

    void Execute() override {
        try {
            pairs_ = engine_->range(start_, end_, limit_);
        } catch (const std::exception& e) {
            SetError(e.what());
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        Napi::Array arr = Napi::Array::New(env, pairs_.size());
        for (size_t i = 0; i < pairs_.size(); i++) {
            Napi::Array pair = Napi::Array::New(env, 2);
            pair.Set((uint32_t)0, Napi::String::New(env, pairs_[i].first));
            pair.Set((uint32_t)1, Napi::String::New(env, pairs_[i].second));
            arr.Set(i, pair);
        }
        deferred.Resolve(arr);
    }

    void OnError(const Napi::Error& e) override {
        deferred.Reject(e.Value());
    }

    Napi::Promise GetPromise() {
        return deferred.Promise();
    }

private:
    Napi::Promise::Deferred deferred;
    titan::TitanEngine* engine_;
    std::string start_;
    std::string end_;
    size_t limit_;
    std::vector<std::pair<std::string, std::string>> pairs_;
};

Napi::Value TitanKV::RangeAsync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2) {
        Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
        deferred.Resolve(Napi::Array::New(env, 0));
        return deferred.Promise();
    }

    std::string start = info[0].As<Napi::String>().Utf8Value();
    std::string end = info[1].As<Napi::String>().Utf8Value();
    size_t limit = 1000;
    if (info.Length() > 2 && info[2].IsNumber()) {
        limit = info[2].As<Napi::Number>().Int64Value();
    }

    RangeAsyncWorker* worker = new RangeAsyncWorker(env, engine_.get(), std::move(start), std::move(end), limit);
    worker->Queue();
    return worker->GetPromise();
}

Napi::Value TitanKV::CountPrefix(const Napi::CallbackInfo& info) {
    if (info.Length() < 1) return Napi::Number::New(info.Env(), 0);
    try {
        return Napi::Number::New(info.Env(), static_cast<double>(engine_->countPrefix(info[0].As<Napi::String>().Utf8Value())));
    } catch (const std::exception& e) {
        Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
        return info.Env().Null();
    }
}

class CountPrefixAsyncWorker : public Napi::AsyncWorker {
public:
    CountPrefixAsyncWorker(Napi::Env& env, titan::TitanEngine* engine, std::string prefix)
        : Napi::AsyncWorker(env),
          deferred(Napi::Promise::Deferred::New(env)),
          engine_(engine),
          prefix_(std::move(prefix)) {}

    ~CountPrefixAsyncWorker() {}

    void Execute() override {
        try {
            count_ = engine_->countPrefix(prefix_);
        } catch (const std::exception& e) {
            SetError(e.what());
        }
    }

    void OnOK() override {
        deferred.Resolve(Napi::Number::New(Env(), static_cast<double>(count_)));
    }

    void OnError(const Napi::Error& e) override {
        deferred.Reject(e.Value());
    }

    Napi::Promise GetPromise() {
        return deferred.Promise();
    }

private:
    Napi::Promise::Deferred deferred;
    titan::TitanEngine* engine_;
    std::string prefix_;
    size_t count_ = 0;
};

Napi::Value TitanKV::CountPrefixAsync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) {
        Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
        deferred.Resolve(Napi::Number::New(env, 0));
        return deferred.Promise();
    }

    std::string prefix = info[0].As<Napi::String>().Utf8Value();
    CountPrefixAsyncWorker* worker = new CountPrefixAsyncWorker(env, engine_.get(), std::move(prefix));
    worker->Queue();
    return worker->GetPromise();
}

Napi::Value TitanKV::PutBatch(const Napi::CallbackInfo& info) {
    if (info.Length() < 1 || !info[0].IsArray()) return info.Env().Undefined();
    Napi::Array arr = info[0].As<Napi::Array>();
    std::vector<std::pair<std::string, std::string>> pairs;
    pairs.reserve(arr.Length());

    for (uint32_t i = 0; i < arr.Length(); i++) {
        Napi::Value item = arr.Get(i);
        if (item.IsArray()) {
            Napi::Array pair = item.As<Napi::Array>();
            if (pair.Length() >= 2) {
                pairs.emplace_back(
                    pair.Get((uint32_t)0).As<Napi::String>().Utf8Value(),
                    pair.Get((uint32_t)1).As<Napi::String>().Utf8Value()
                );
            }
        }
    }
    try {
        engine_->putBatch(pairs);
    } catch (const std::exception& e) {
        Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
    }
    return info.Env().Undefined();
}

Napi::Value TitanKV::GetBatch(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsArray()) return Napi::Array::New(env, 0);
    Napi::Array arr = info[0].As<Napi::Array>();
    std::vector<std::string> keys;
    keys.reserve(arr.Length());

    for (uint32_t i = 0; i < arr.Length(); i++) {
        keys.push_back(arr.Get(i).As<Napi::String>().Utf8Value());
    }

    try {
        auto results = engine_->getBatch(keys);
        Napi::Array out = Napi::Array::New(env, results.size());
        for (size_t i = 0; i < results.size(); i++) {
            out.Set(i, results[i] ? Napi::String::New(env, *results[i]) : env.Null());
        }
        return out;
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

class PutBatchAsyncWorker : public Napi::AsyncWorker {
public:
    PutBatchAsyncWorker(
        Napi::Env& env,
        titan::TitanEngine* engine,
        std::vector<std::pair<std::string, std::string>> pairs)
        : Napi::AsyncWorker(env),
          deferred(Napi::Promise::Deferred::New(env)),
          engine_(engine),
          pairs_(std::move(pairs)) {}

    ~PutBatchAsyncWorker() {}

    void Execute() override {
        try {
            engine_->putBatch(pairs_);
        } catch (const std::exception& e) {
            SetError(e.what());
        }
    }

    void OnOK() override {
        deferred.Resolve(Env().Undefined());
    }

    void OnError(const Napi::Error& e) override {
        deferred.Reject(e.Value());
    }

    Napi::Promise GetPromise() {
        return deferred.Promise();
    }

private:
    Napi::Promise::Deferred deferred;
    titan::TitanEngine* engine_;
    std::vector<std::pair<std::string, std::string>> pairs_;
};

Napi::Value TitanKV::PutBatchAsync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsArray()) {
        Napi::TypeError::New(env, "Expected array of key-value pairs").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Array arr = info[0].As<Napi::Array>();
    std::vector<std::pair<std::string, std::string>> pairs;
    pairs.reserve(arr.Length());

    for (uint32_t i = 0; i < arr.Length(); i++) {
        Napi::Value item = arr.Get(i);
        if (item.IsArray()) {
            Napi::Array pair = item.As<Napi::Array>();
            if (pair.Length() >= 2) {
                pairs.emplace_back(
                    pair.Get((uint32_t)0).As<Napi::String>().Utf8Value(),
                    pair.Get((uint32_t)1).As<Napi::String>().Utf8Value()
                );
            }
        }
    }

    PutBatchAsyncWorker* worker = new PutBatchAsyncWorker(env, engine_.get(), std::move(pairs));
    worker->Queue();
    return worker->GetPromise();
}

class GetBatchAsyncWorker : public Napi::AsyncWorker {
public:
    GetBatchAsyncWorker(
        Napi::Env& env,
        titan::TitanEngine* engine,
        std::vector<std::string> keys)
        : Napi::AsyncWorker(env),
          deferred(Napi::Promise::Deferred::New(env)),
          engine_(engine),
          keys_(std::move(keys)) {}

    ~GetBatchAsyncWorker() {}

    void Execute() override {
        try {
            results_ = engine_->getBatch(keys_);
        } catch (const std::exception& e) {
            SetError(e.what());
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        Napi::Array out = Napi::Array::New(env, results_.size());
        for (size_t i = 0; i < results_.size(); i++) {
            out.Set(i, results_[i] ? Napi::String::New(env, *results_[i]) : env.Null());
        }
        deferred.Resolve(out);
    }

    void OnError(const Napi::Error& e) override {
        deferred.Reject(e.Value());
    }

    Napi::Promise GetPromise() {
        return deferred.Promise();
    }

private:
    Napi::Promise::Deferred deferred;
    titan::TitanEngine* engine_;
    std::vector<std::string> keys_;
    std::vector<std::optional<std::string>> results_;
};

Napi::Value TitanKV::GetBatchAsync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsArray()) {
        Napi::TypeError::New(env, "Expected array of keys").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Array arr = info[0].As<Napi::Array>();
    std::vector<std::string> keys;
    keys.reserve(arr.Length());

    for (uint32_t i = 0; i < arr.Length(); i++) {
        keys.push_back(arr.Get(i).As<Napi::String>().Utf8Value());
    }

    GetBatchAsyncWorker* worker = new GetBatchAsyncWorker(env, engine_.get(), std::move(keys));
    worker->Queue();
    return worker->GetPromise();
}

class FlushAsyncWorker : public Napi::AsyncWorker {
public:
    FlushAsyncWorker(Napi::Env& env, titan::TitanEngine* engine)
        : Napi::AsyncWorker(env), deferred(Napi::Promise::Deferred::New(env)), engine_(engine) {}

    ~FlushAsyncWorker() {}

    void Execute() override {
        try {
            engine_->flush();
        } catch (const std::exception& e) {
            SetError(e.what());
        }
    }

    void OnOK() override {
        deferred.Resolve(Env().Undefined());
    }

    void OnError(const Napi::Error& e) override {
        deferred.Reject(e.Value());
    }

    Napi::Promise GetPromise() {
        return deferred.Promise();
    }

private:
    Napi::Promise::Deferred deferred;
    titan::TitanEngine* engine_;
};

class CompactAsyncWorker : public Napi::AsyncWorker {
public:
    CompactAsyncWorker(Napi::Env& env, titan::TitanEngine* engine)
        : Napi::AsyncWorker(env), deferred(Napi::Promise::Deferred::New(env)), engine_(engine) {}

    ~CompactAsyncWorker() {}

    void Execute() override {
        try {
            engine_->compact();
        } catch (const std::exception& e) {
            SetError(e.what());
        }
    }

    void OnOK() override {
        deferred.Resolve(Env().Undefined());
    }

    void OnError(const Napi::Error& e) override {
        deferred.Reject(e.Value());
    }

    Napi::Promise GetPromise() {
        return deferred.Promise();
    }

private:
    Napi::Promise::Deferred deferred;
    titan::TitanEngine* engine_;
};

Napi::Value TitanKV::Flush(const Napi::CallbackInfo& info) {
    try {
        engine_->flush();
    } catch (const std::exception& e) {
        Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
    }
    return info.Env().Undefined();
}

Napi::Value TitanKV::FlushAsync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    FlushAsyncWorker* worker = new FlushAsyncWorker(env, engine_.get());
    worker->Queue();
    return worker->GetPromise();
}

Napi::Value TitanKV::Compact(const Napi::CallbackInfo& info) {
    try {
        engine_->compact();
    } catch (const std::exception& e) {
        Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
    }
    return info.Env().Undefined();
}

Napi::Value TitanKV::CompactAsync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    CompactAsyncWorker* worker = new CompactAsyncWorker(env, engine_.get());
    worker->Queue();
    return worker->GetPromise();
}

Napi::Value TitanKV::Close(const Napi::CallbackInfo& info) {
    try {
        if (engine_) engine_->close();
    } catch (const std::exception& e) {
        Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
    }
    return info.Env().Undefined();
}

Napi::Value TitanKV::GetStats(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    try {
        auto stats = engine_->getStats();
        Napi::Object obj = Napi::Object::New(env);
        obj.Set("keyCount", Napi::Number::New(env, (double)stats.key_count));
        obj.Set("rawBytes", Napi::Number::New(env, (double)stats.raw_bytes));
        obj.Set("compressedBytes", Napi::Number::New(env, (double)stats.compressed_bytes));
        obj.Set("walBytes", Napi::Number::New(env, (double)stats.wal_size_bytes));
        obj.Set("logicalWriteBytes", Napi::Number::New(env, (double)stats.logical_write_bytes));
        obj.Set("physicalWriteBytes", Napi::Number::New(env, (double)stats.physical_write_bytes));
        obj.Set("compactionCount", Napi::Number::New(env, (double)stats.compaction_count));
        obj.Set("autoCompactionCount", Napi::Number::New(env, (double)stats.auto_compaction_count));
        obj.Set("writeAmplification", Napi::Number::New(env, stats.write_amplification));
        obj.Set("spaceAmplification", Napi::Number::New(env, stats.space_amplification));

        double ratio = stats.raw_bytes > 0 ? (double)stats.compressed_bytes / stats.raw_bytes : 0.0;
        obj.Set("compressionRatio", Napi::Number::New(env, ratio));

        return obj;
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    return TitanKV::Init(env, exports);
}

NODE_API_MODULE(titankv, Init)
