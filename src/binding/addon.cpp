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
    Napi::Value Get(const Napi::CallbackInfo& info);
    Napi::Value Del(const Napi::CallbackInfo& info);
    Napi::Value Has(const Napi::CallbackInfo& info);
    Napi::Value Size(const Napi::CallbackInfo& info);
    Napi::Value Clear(const Napi::CallbackInfo& info);
    Napi::Value Incr(const Napi::CallbackInfo& info);
    Napi::Value Decr(const Napi::CallbackInfo& info);
    Napi::Value Keys(const Napi::CallbackInfo& info);
    Napi::Value Scan(const Napi::CallbackInfo& info);
    Napi::Value Range(const Napi::CallbackInfo& info);
    Napi::Value CountPrefix(const Napi::CallbackInfo& info);
    Napi::Value PutBatch(const Napi::CallbackInfo& info);
    Napi::Value GetBatch(const Napi::CallbackInfo& info);
    Napi::Value Flush(const Napi::CallbackInfo& info);
    Napi::Value Compact(const Napi::CallbackInfo& info);
    Napi::Value GetStats(const Napi::CallbackInfo& info);
};

Napi::Object TitanKV::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "TitanKV", {
        InstanceMethod("put", &TitanKV::Put),
        InstanceMethod("get", &TitanKV::Get),
        InstanceMethod("del", &TitanKV::Del),
        InstanceMethod("has", &TitanKV::Has),
        InstanceMethod("size", &TitanKV::Size),
        InstanceMethod("clear", &TitanKV::Clear),
        InstanceMethod("incr", &TitanKV::Incr),
        InstanceMethod("decr", &TitanKV::Decr),
        InstanceMethod("keys", &TitanKV::Keys),
        InstanceMethod("scan", &TitanKV::Scan),
        InstanceMethod("range", &TitanKV::Range),
        InstanceMethod("countPrefix", &TitanKV::CountPrefix),
        InstanceMethod("putBatch", &TitanKV::PutBatch),
        InstanceMethod("getBatch", &TitanKV::GetBatch),
        InstanceMethod("flush", &TitanKV::Flush),
        InstanceMethod("compact", &TitanKV::Compact),
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

    if (info.Length() > 0 && info[0].IsString()) {
        path = info[0].As<Napi::String>().Utf8Value();
    }
    
    if (info.Length() > 1 && info[1].IsObject()) {
        Napi::Object opts = info[1].As<Napi::Object>();
        if (opts.Has("compressionLevel")) {
            compression_level = opts.Get("compressionLevel").As<Napi::Number>().Int32Value();
        }
    }

    engine_ = std::make_unique<titan::TitanEngine>(path);
    engine_->setCompressionLevel(compression_level);
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
    engine_->put(key, value, ttl);
    return env.Undefined();
}

Napi::Value TitanKV::Get(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return env.Null();
    auto result = engine_->get(info[0].As<Napi::String>().Utf8Value());
    return result ? Napi::String::New(env, *result) : env.Null();
}

Napi::Value TitanKV::Del(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return Napi::Boolean::New(env, false);
    return Napi::Boolean::New(env, engine_->del(info[0].As<Napi::String>().Utf8Value()));
}

Napi::Value TitanKV::Has(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return Napi::Boolean::New(env, false);
    return Napi::Boolean::New(env, engine_->has(info[0].As<Napi::String>().Utf8Value()));
}

Napi::Value TitanKV::Size(const Napi::CallbackInfo& info) {
    return Napi::Number::New(info.Env(), static_cast<double>(engine_->size()));
}

Napi::Value TitanKV::Clear(const Napi::CallbackInfo& info) {
    engine_->clear();
    return info.Env().Undefined();
}

Napi::Value TitanKV::Incr(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    int64_t delta = 1;
    if (info.Length() > 1 && info[1].IsNumber()) delta = info[1].As<Napi::Number>().Int64Value();
    return Napi::Number::New(env, static_cast<double>(engine_->incr(info[0].As<Napi::String>().Utf8Value(), delta)));
}

Napi::Value TitanKV::Decr(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    int64_t delta = 1;
    if (info.Length() > 1 && info[1].IsNumber()) delta = info[1].As<Napi::Number>().Int64Value();
    return Napi::Number::New(env, static_cast<double>(engine_->decr(info[0].As<Napi::String>().Utf8Value(), delta)));
}

Napi::Value TitanKV::Keys(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    size_t limit = 1000;
    if (info.Length() > 0 && info[0].IsNumber()) limit = info[0].As<Napi::Number>().Int64Value();
    
    auto keys = engine_->keys(limit);
    Napi::Array arr = Napi::Array::New(env, keys.size());
    for (size_t i = 0; i < keys.size(); i++) {
        arr.Set(i, Napi::String::New(env, keys[i]));
    }
    return arr;
}

Napi::Value TitanKV::Scan(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return Napi::Array::New(env, 0);
    std::string prefix = info[0].As<Napi::String>().Utf8Value();
    size_t limit = 1000;
    if (info.Length() > 1 && info[1].IsNumber()) limit = info[1].As<Napi::Number>().Int64Value();

    auto pairs = engine_->scan(prefix, limit);
    Napi::Array arr = Napi::Array::New(env, pairs.size());
    for (size_t i = 0; i < pairs.size(); i++) {
        Napi::Array pair = Napi::Array::New(env, 2);
        pair.Set((uint32_t)0, Napi::String::New(env, pairs[i].first));
        pair.Set((uint32_t)1, Napi::String::New(env, pairs[i].second));
        arr.Set(i, pair);
    }
    return arr;
}

Napi::Value TitanKV::Range(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2) return Napi::Array::New(env, 0);
    std::string start = info[0].As<Napi::String>().Utf8Value();
    std::string end = info[1].As<Napi::String>().Utf8Value();
    size_t limit = 1000;
    if (info.Length() > 2 && info[2].IsNumber()) limit = info[2].As<Napi::Number>().Int64Value();

    auto pairs = engine_->range(start, end, limit);
    Napi::Array arr = Napi::Array::New(env, pairs.size());
    for (size_t i = 0; i < pairs.size(); i++) {
        Napi::Array pair = Napi::Array::New(env, 2);
        pair.Set((uint32_t)0, Napi::String::New(env, pairs[i].first));
        pair.Set((uint32_t)1, Napi::String::New(env, pairs[i].second));
        arr.Set(i, pair);
    }
    return arr;
}

Napi::Value TitanKV::CountPrefix(const Napi::CallbackInfo& info) {
    if (info.Length() < 1) return Napi::Number::New(info.Env(), 0);
    return Napi::Number::New(info.Env(), static_cast<double>(engine_->countPrefix(info[0].As<Napi::String>().Utf8Value())));
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
    engine_->putBatch(pairs);
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

    auto results = engine_->getBatch(keys);
    Napi::Array out = Napi::Array::New(env, results.size());
    for (size_t i = 0; i < results.size(); i++) {
        out.Set(i, results[i] ? Napi::String::New(env, *results[i]) : env.Null());
    }
    return out;
}

Napi::Value TitanKV::Flush(const Napi::CallbackInfo& info) {
    engine_->flush();
    return info.Env().Undefined();
}

Napi::Value TitanKV::Compact(const Napi::CallbackInfo& info) {
    engine_->compact();
    return info.Env().Undefined();
}

Napi::Value TitanKV::GetStats(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    auto stats = engine_->getStats();
    Napi::Object obj = Napi::Object::New(env);
    obj.Set("keyCount", Napi::Number::New(env, (double)stats.key_count));
    obj.Set("rawBytes", Napi::Number::New(env, (double)stats.raw_bytes));
    obj.Set("compressedBytes", Napi::Number::New(env, (double)stats.compressed_bytes));
    
    double ratio = stats.raw_bytes > 0 ? (double)stats.compressed_bytes / stats.raw_bytes : 0.0;
    obj.Set("compressionRatio", Napi::Number::New(env, ratio));
    
    return obj;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    return TitanKV::Init(env, exports);
}

NODE_API_MODULE(titankv, Init)
