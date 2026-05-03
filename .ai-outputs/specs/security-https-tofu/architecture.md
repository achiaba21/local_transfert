# Architecture — Sprint V1.6.4 Sécurité

**Date :** 2026-05-03
**Statut :** ✅ Validée

## LOT 1 — HTTPS LAN

### CMakeLists
```cmake
find_package(OpenSSL REQUIRED)
target_compile_definitions(ltr_core PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(ltr_core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
```
Sur Mac, `find_package` détecte automatiquement /opt/homebrew/opt/openssl@3.

### cert_manager.hpp/.cpp
```cpp
struct CertPair {
    std::string certPem;
    std::string keyPem;
    std::string fingerprintSha256;  // "AB:CD:EF:..."
};
CertPair loadOrGenerate(const std::filesystem::path& cfgDir);
```
- Si `cert.pem` + `key.pem` existent : load + recompute fingerprint
- Sinon : génère RSA 2048 + X509 self-signed CN=LocalTransfer, validity
  5 ans (157680000 secondes), sauvegarde en PEM
- Fingerprint = SHA-256 du DER cert, format hex avec ':' tous les 2
  octets, majuscules

### WebService 2 servers
- HTTP server existant sur 45456 (inchangé)
- Nouveau `httplib::SSLServer(certPem, keyPem)` sur 45457
- Si 45457 occupé, fallback 45458, 45459
- Routes registrées via lambda générique sur les deux
- Threads séparés (1 par server), join au shutdown

### /api/cert-info
- Public (pas d'auth)
- Retourne `{ "fingerprint": "AB:CD:..." }`

### Bandeau /login
- HTML : `<div id="cert-banner">`
- JS : si `https:` détecté, fetch `/api/cert-info`, affiche bandeau
- Checkbox `#cert-trusted` **pré-cochée** par défaut
- localStorage `ltr-cert-trusted-fingerprint` mémorise
- Si fingerprint mémorisé == fingerprint actuel → bandeau **caché**
- Sinon affiché avec checkbox active mais cochée

### SharePanel fingerprint
- `setFingerprint(string)` ajoute une ligne sous le PIN
- Format : 8 lignes de 8 octets ou 4 lignes de 16 octets
- Bouton Copier réutilise pattern existant

## LOT 2 — TOFU Ed25519

### crypto_identity.hpp/.cpp (nouveau)
```cpp
struct Identity {
    std::string deviceId;        // hex
    std::vector<uint8_t> pubKey; // 32 bytes Ed25519
    std::vector<uint8_t> privKey;// 32 bytes Ed25519 (jamais transmis)
};
Identity loadOrGenerate(const std::filesystem::path& cfgDir);
std::vector<uint8_t> sign(const Identity& id, std::string_view msg);
bool verify(const std::vector<uint8_t>& pubKey,
            std::string_view msg,
            const std::vector<uint8_t>& signature);
std::string pubKeyFingerprint(const std::vector<uint8_t>& pubKey);
```
- Stockage `cfgDir/identity.json` : { deviceId, pubKey hex, privKey hex }
- Génération via openssl EVP_PKEY_ED25519 (RFC 8032)
- Sign 64 bytes signature output
- Verify retourne bool

### known_peers.hpp/.cpp (nouveau)
```cpp
class KnownPeers {
public:
    enum class SetResult { New, Same, Changed };
    std::optional<std::string> get(std::string_view peerId) const;
    SetResult set(std::string_view peerId, std::string_view fingerprint);
    void save();
    void purgeOlderThan(int days);  // V1.6.4 : 365 jours
private:
    struct Entry { std::string fingerprint; std::int64_t lastSeenSec; };
    std::map<std::string, Entry> map_;
    std::filesystem::path path_;
};
```
- Stockage `cfgDir/known_peers.json`
- Mac : `~/Library/Application Support/LocalTransfer/`
- Win : `%APPDATA%\LocalTransfer\`

### Protocole TCP étendu
- Sender envoie `OFFER` avec `pubKey` (hex) + `signature` (hex) de
  `nonce` (random 32 bytes injecté dans OFFER aussi)
- Receveur vérifie : signature valide ? Si oui, fingerprint =
  pubKeyFingerprint(pubKey)
- Receveur ajoute `peerFingerprint` dans Accept JSON
- Sender lookup KnownPeers :
  - New → store TOFU, log
  - Same → OK
  - Changed → poste FingerprintChangedEvent → AppController set
    `peer.fingerprintWarning = true`

### UI inline warning
- `device_list_item.cpp` : si `peer.fingerprintWarning`, affiche
  emoji ⚠ orange à droite du nom
- Tooltip si hover (V2)

### Tests
- `test_cert_manager` : roundtrip load → save → re-load, fingerprint
  cohérent
- `test_known_peers` : SetResult sequences, purge TTL
- `test_crypto_identity` : sign/verify roundtrip, deviceId stable

## Fichiers AJOUTER (~7)
- include/ltr/web/cert_manager.hpp + src/web/cert_manager.cpp
- include/ltr/infra/crypto_identity.hpp + src/infra/crypto_identity.cpp
- include/ltr/infra/known_peers.hpp + src/infra/known_peers.cpp
- tests/test_cert_manager.cpp
- tests/test_known_peers.cpp
- tests/test_crypto_identity.cpp

## Fichiers MODIFIER (~10)
- CMakeLists.txt + cmake/Dependencies.cmake
- WebService .hpp + .cpp (2 servers)
- routes/auth_routes.cpp (/api/cert-info)
- assets/web/login.html + login.js (bandeau)
- assets/web/css/style.css (bandeau styles)
- share_panel.hpp + .cpp (fingerprint line)
- main_screen.cpp (sync fingerprint)
- transfer_server.cpp + transfer_client.cpp (Ed25519 protocole)
- event_bus.hpp (FingerprintChangedEvent)
- app_controller.cpp (handle event)
- device.hpp (fingerprintWarning)
- device_list_item.cpp (warning emoji)

UI_REQUIRED: true (bandeau, fingerprint, warning)
