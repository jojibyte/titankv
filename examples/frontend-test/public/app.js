// Author: joji
'use strict';

const statusEl = document.getElementById('status');
const getResultEl = document.getElementById('get-result');
const keysListEl = document.getElementById('keys-list');

function setStatus(message, isError = false) {
  statusEl.textContent = message;
  statusEl.dataset.state = isError ? 'error' : 'ok';
}

async function request(url, options = {}, timeoutMs = 5000) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const res = await fetch(url, { ...options, signal: controller.signal });
    const payload = await res.json();
    if (!res.ok) {
      throw new Error(payload.error || `HTTP ${res.status}`);
    }
    return payload;
  } finally {
    clearTimeout(timeout);
  }
}

document.getElementById('put-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  const form = event.currentTarget;
  const key = form.key.value.trim();
  const value = form.value.value;
  const ttl = Number(form.ttl.value);

  try {
    await request('/api/put', {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ key, value, ttlMs: Number.isFinite(ttl) ? ttl : 0 })
    });
    setStatus(`Kaydedildi: ${key}`);
    form.reset();
  } catch (error) {
    setStatus(`Put hata: ${error.message}`, true);
  }
});

document.getElementById('get-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  const key = event.currentTarget.key.value.trim();

  try {
    const payload = await request(`/api/get?key=${encodeURIComponent(key)}`);
    getResultEl.textContent = payload.value === null ? 'Bulunamadı' : `Değer: ${payload.value}`;
    setStatus(`Get tamamlandı: ${key}`);
  } catch (error) {
    setStatus(`Get hata: ${error.message}`, true);
  }
});

document.getElementById('del-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  const key = event.currentTarget.key.value.trim();

  try {
    const payload = await request(`/api/key?key=${encodeURIComponent(key)}`, { method: 'DELETE' });
    setStatus(payload.deleted ? `Silindi: ${key}` : `Silinecek key bulunamadı: ${key}`);
  } catch (error) {
    setStatus(`Delete hata: ${error.message}`, true);
  }
});

document.getElementById('keys-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  const prefix = event.currentTarget.prefix.value.trim();

  try {
    const payload = await request(`/api/keys?prefix=${encodeURIComponent(prefix)}`);
    keysListEl.textContent = '';
    for (const key of payload.keys) {
      const li = document.createElement('li');
      li.textContent = key;
      keysListEl.appendChild(li);
    }
    setStatus(`Toplam ${payload.keys.length} key listelendi.`);
  } catch (error) {
    setStatus(`Keys hata: ${error.message}`, true);
  }
});
