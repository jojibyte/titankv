#include <napi.h>
#include "titankv.hpp"
#include "wal.hpp"
#include <memory>

class TitanKV : public Napi::ObjectWrap<TitanKV> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    TitanKV(const Napi::CallbackInfo& info);

private:
    std::unique_ptr<titan::TitanEngine> engine_;

    // core
    Napi::Value Put(const Napi::CallbackInfo& info);
    Napi::Value Get(const Napi::CallbackInfo& info);
    Napi::Value Del(const Napi::CallbackInfo& info);
    Napi::Value Has(const Napi::CallbackInfo& info);
    Napi::Value Size(const Napi::CallbackInfo& info);
    Napi::Value Clear(const Napi::CallbackInfo& info);
    Napi::Value Incr(const Napi::CallbackInfo& info);
    Napi::Value Decr(const Napi::CallbackInfo& info);
    
    // query
    Napi::Value Keys(const Napi::CallbackInfo& info);
    Napi::Value Scan(const Napi::CallbackInfo& info);
    Napi::Value Range(const Napi::CallbackInfo& info);
    Napi::Value CountPrefix(const Napi::CallbackInfo& info);
    
    // batch
    Napi::Value PutBatch(const Napi::CallbackInfo& info);
    Napi::Value GetBatch(const Napi::CallbackInfo& info);
    
    // list
    Napi::Value Lpush(const Napi::CallbackInfo& info);
    Napi::Value Rpush(const Napi::CallbackInfo& info);
    Napi::Value Lpop(const Napi::CallbackInfo& info);
    Napi::Value Rpop(const Napi::CallbackInfo& info);
    Napi::Value Lrange(const Napi::CallbackInfo& info);
    Napi::Value Llen(const Napi::CallbackInfo& info);
    
    // set
    Napi::Value Sadd(const Napi::CallbackInfo& info);
    Napi::Value Srem(const Napi::CallbackInfo& info);
    Napi::Value Sismember(const Napi::CallbackInfo& info);
    Napi::Value Smembers(const Napi::CallbackInfo& info);
    Napi::Value Scard(const Napi::CallbackInfo& info);
    
    // util
    Napi::Value Flush(const Napi::CallbackInfo& info);
    Napi::Value GetStats(const Napi::CallbackInfo& info);
};

Napi::Object TitanKV::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "TitanKV", {
        // core
        InstanceMethod("put", &TitanKV::Put),
        InstanceMethod("get", &TitanKV::Get),
        InstanceMethod("del", &TitanKV::Del),
        InstanceMethod("has", &TitanKV::Has),
        InstanceMethod("size", &TitanKV::Size),
        InstanceMethod("clear", &TitanKV::Clear),
        InstanceMethod("incr", &TitanKV::Incr),
        InstanceMethod("decr", &TitanKV::Decr),
        // query
        InstanceMethod("keys", &TitanKV::Keys),
        InstanceMethod("scan", &TitanKV::Scan),
        InstanceMethod("range", &TitanKV::Range),
        InstanceMethod("countPrefix", &TitanKV::CountPrefix),
        // batch
        InstanceMethod("putBatch", &TitanKV::PutBatch),
        InstanceMethod("getBatch", &TitanKV::GetBatch),
        // list
        InstanceMethod("lpush", &TitanKV::Lpush),
        InstanceMethod("rpush", &TitanKV::Rpush),
        InstanceMethod("lpop", &TitanKV::Lpop),
        InstanceMethod("rpop", &TitanKV::Rpop),
        InstanceMethod("lrange", &TitanKV::Lrange),
        InstanceMethod("llen", &TitanKV::Llen),
        // set
        InstanceMethod("sadd", &TitanKV::Sadd),
        InstanceMethod("srem", &TitanKV::Srem),
        InstanceMethod("sismember", &TitanKV::Sismember),
        InstanceMethod("smembers", &TitanKV::Smembers),
        InstanceMethod("scard", &TitanKV::Scard),
        // util
        InstanceMethod("flush", &TitanKV::Flush),
        InstanceMethod("stats", &TitanKV::GetStats),
    });

    Napi::FunctionReference* constructor = new Napi::FunctionReference();
    *constructor = Napi::Persistent(func);
    env.SetInstanceData(constructor);

    exports.Set("TitanKV", func);
    return exports;
}

TitanKV::TitanKV(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<TitanKV>(info) {
    if (info.Length() > 0 && info[0].IsString()) {
        std::string path = info[0].As<Napi::String>().Utf8Value();
        int sync_mode = 1;
        if (info.Length() > 1 && info[1].IsObject()) {
            Napi::Object opts = info[1].As<Napi::Object>();
            if (opts.Has("sync")) {
                std::string s = opts.Get("sync").As<Napi::String>().Utf8Value();
                if (s == "sync") sync_mode = 0;
                else if (s == "none") sync_mode = 2;
            }
        }
        engine_ = std::make_unique<titan::TitanEngine>(path, sync_mode);
    } else {
        engine_ = std::make_unique<titan::TitanEngine>();
    }
}

// core ops
Napi::Value TitanKV::Put(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2) {
        Napi::TypeError::New(env, "expected key and value").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    std::string key = info[0].As<Napi::String>().Utf8Value();
    std::string value = info[1].As<Napi::String>().Utf8Value();
    int64_t ttl = info.Length() > 2 && info[2].IsNumber() ? info[2].As<Napi::Number>().Int64Value() : 0;
    engine_->put(key, value, ttl);
    return env.Undefined();
}

Napi::Value TitanKV::Get(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) {
        Napi::TypeError::New(env, "expected key").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    auto result = engine_->get(info[0].As<Napi::String>().Utf8Value());
    return result ? Napi::String::New(env, *result) : env.Null();
}

Napi::Value TitanKV::Del(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return env.Undefined();
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
    if (info.Length() < 1) return env.Undefined();
    int64_t delta = info.Length() > 1 && info[1].IsNumber() ? info[1].As<Napi::Number>().Int64Value() : 1;
    return Napi::Number::New(env, static_cast<double>(engine_->incr(info[0].As<Napi::String>().Utf8Value(), delta)));
}

Napi::Value TitanKV::Decr(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return env.Undefined();
    int64_t delta = info.Length() > 1 && info[1].IsNumber() ? info[1].As<Napi::Number>().Int64Value() : 1;
    return Napi::Number::New(env, static_cast<double>(engine_->decr(info[0].As<Napi::String>().Utf8Value(), delta)));
}

// query ops
Napi::Value TitanKV::Keys(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    auto keys = engine_->keys();
    Napi::Array arr = Napi::Array::New(env, keys.size());
    for (size_t i = 0; i < keys.size(); i++) arr.Set(i, Napi::String::New(env, keys[i]));
    return arr;
}

Napi::Value TitanKV::Scan(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return Napi::Array::New(env, 0);
    auto results = engine_->scan(info[0].As<Napi::String>().Utf8Value());
    Napi::Array arr = Napi::Array::New(env, results.size());
    for (size_t i = 0; i < results.size(); i++) {
        Napi::Array pair = Napi::Array::New(env, 2);
        pair.Set((uint32_t)0, Napi::String::New(env, results[i].first));
        pair.Set((uint32_t)1, Napi::String::New(env, results[i].second));
        arr.Set(i, pair);
    }
    return arr;
}

Napi::Value TitanKV::Range(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2) return Napi::Array::New(env, 0);
    auto results = engine_->range(info[0].As<Napi::String>().Utf8Value(), info[1].As<Napi::String>().Utf8Value());
    Napi::Array arr = Napi::Array::New(env, results.size());
    for (size_t i = 0; i < results.size(); i++) {
        Napi::Array pair = Napi::Array::New(env, 2);
        pair.Set((uint32_t)0, Napi::String::New(env, results[i].first));
        pair.Set((uint32_t)1, Napi::String::New(env, results[i].second));
        arr.Set(i, pair);
    }
    return arr;
}

Napi::Value TitanKV::CountPrefix(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return Napi::Number::New(env, 0);
    return Napi::Number::New(env, static_cast<double>(engine_->countPrefix(info[0].As<Napi::String>().Utf8Value())));
}

// batch
Napi::Value TitanKV::PutBatch(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsArray()) return env.Undefined();
    Napi::Array arr = info[0].As<Napi::Array>();
    std::vector<titan::TitanEngine::KVPair> pairs;
    pairs.reserve(arr.Length());
    for (uint32_t i = 0; i < arr.Length(); i++) {
        Napi::Value item = arr.Get(i);
        if (!item.IsArray()) continue;
        Napi::Array pair = item.As<Napi::Array>();
        if (pair.Length() < 2) continue;
        pairs.emplace_back(pair.Get((uint32_t)0).As<Napi::String>().Utf8Value(), pair.Get((uint32_t)1).As<Napi::String>().Utf8Value());
    }
    engine_->putBatch(pairs);
    return env.Undefined();
}

Napi::Value TitanKV::GetBatch(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsArray()) return Napi::Array::New(env, 0);
    Napi::Array arr = info[0].As<Napi::Array>();
    std::vector<std::string> keys;
    keys.reserve(arr.Length());
    for (uint32_t i = 0; i < arr.Length(); i++) keys.push_back(arr.Get(i).As<Napi::String>().Utf8Value());
    auto results = engine_->getBatch(keys);
    Napi::Array output = Napi::Array::New(env, results.size());
    for (size_t i = 0; i < results.size(); i++) {
        output.Set(i, results[i] ? Napi::String::New(env, *results[i]) : env.Null());
    }
    return output;
}

// list ops
Napi::Value TitanKV::Lpush(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2) return Napi::Number::New(env, 0);
    return Napi::Number::New(env, static_cast<double>(engine_->lpush(info[0].As<Napi::String>().Utf8Value(), info[1].As<Napi::String>().Utf8Value())));
}

Napi::Value TitanKV::Rpush(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2) return Napi::Number::New(env, 0);
    return Napi::Number::New(env, static_cast<double>(engine_->rpush(info[0].As<Napi::String>().Utf8Value(), info[1].As<Napi::String>().Utf8Value())));
}

Napi::Value TitanKV::Lpop(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return env.Null();
    auto result = engine_->lpop(info[0].As<Napi::String>().Utf8Value());
    return result ? Napi::String::New(env, *result) : env.Null();
}

Napi::Value TitanKV::Rpop(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return env.Null();
    auto result = engine_->rpop(info[0].As<Napi::String>().Utf8Value());
    return result ? Napi::String::New(env, *result) : env.Null();
}

Napi::Value TitanKV::Lrange(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 3) return Napi::Array::New(env, 0);
    auto results = engine_->lrange(info[0].As<Napi::String>().Utf8Value(), info[1].As<Napi::Number>().Int32Value(), info[2].As<Napi::Number>().Int32Value());
    Napi::Array arr = Napi::Array::New(env, results.size());
    for (size_t i = 0; i < results.size(); i++) arr.Set(i, Napi::String::New(env, results[i]));
    return arr;
}

Napi::Value TitanKV::Llen(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return Napi::Number::New(env, 0);
    return Napi::Number::New(env, static_cast<double>(engine_->llen(info[0].As<Napi::String>().Utf8Value())));
}

// set ops
Napi::Value TitanKV::Sadd(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2) return Napi::Number::New(env, 0);
    return Napi::Number::New(env, static_cast<double>(engine_->sadd(info[0].As<Napi::String>().Utf8Value(), info[1].As<Napi::String>().Utf8Value())));
}

Napi::Value TitanKV::Srem(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2) return Napi::Boolean::New(env, false);
    return Napi::Boolean::New(env, engine_->srem(info[0].As<Napi::String>().Utf8Value(), info[1].As<Napi::String>().Utf8Value()));
}

Napi::Value TitanKV::Sismember(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2) return Napi::Boolean::New(env, false);
    return Napi::Boolean::New(env, engine_->sismember(info[0].As<Napi::String>().Utf8Value(), info[1].As<Napi::String>().Utf8Value()));
}

Napi::Value TitanKV::Smembers(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return Napi::Array::New(env, 0);
    auto members = engine_->smembers(info[0].As<Napi::String>().Utf8Value());
    Napi::Array arr = Napi::Array::New(env, members.size());
    for (size_t i = 0; i < members.size(); i++) arr.Set(i, Napi::String::New(env, members[i]));
    return arr;
}

Napi::Value TitanKV::Scard(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) return Napi::Number::New(env, 0);
    return Napi::Number::New(env, static_cast<double>(engine_->scard(info[0].As<Napi::String>().Utf8Value())));
}

// util
Napi::Value TitanKV::Flush(const Napi::CallbackInfo& info) {
    engine_->flush();
    return info.Env().Undefined();
}

Napi::Value TitanKV::GetStats(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    auto s = engine_->getStats();
    Napi::Object obj = Napi::Object::New(env);
    obj.Set("totalKeys", Napi::Number::New(env, static_cast<double>(s.total_keys)));
    obj.Set("totalOps", Napi::Number::New(env, static_cast<double>(s.total_ops)));
    obj.Set("hits", Napi::Number::New(env, static_cast<double>(s.hits)));
    obj.Set("misses", Napi::Number::New(env, static_cast<double>(s.misses)));
    obj.Set("expired", Napi::Number::New(env, static_cast<double>(s.expired)));
    obj.Set("hitRate", s.hits + s.misses > 0 ? Napi::Number::New(env, static_cast<double>(s.hits) / (s.hits + s.misses)) : Napi::Number::New(env, 0));
    return obj;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    return TitanKV::Init(env, exports);
}

NODE_API_MODULE(titankv, Init)
