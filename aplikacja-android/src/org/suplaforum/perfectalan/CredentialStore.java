package org.suplaforum.perfectalan;

import android.content.Context;
import android.content.SharedPreferences;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.Base64;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

final class CredentialStore {
    static final String ALIAS = "org.suplaforum.perfectalan.password";
    final SharedPreferences preferences;
    CredentialStore(Context context) { preferences = context.getSharedPreferences("connection", Context.MODE_PRIVATE); }
    String endpoint() { return preferences.getString("endpoint", "http://perfecta-esp.local"); }
    String username() { return preferences.getString("username", "admin"); }
    boolean configured() { return preferences.contains("password"); }
    boolean cloudConfigured(){return preferences.contains("supla_token");}
    private SecretKey key() throws Exception {
        KeyStore store = KeyStore.getInstance("AndroidKeyStore"); store.load(null);
        if (store.containsAlias(ALIAS)) return (SecretKey)store.getKey(ALIAS, null);
        KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        generator.init(new KeyGenParameterSpec.Builder(ALIAS, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM).setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setKeySize(256).build());
        return generator.generateKey();
    }
    String password() throws Exception {
        return decrypt(preferences.getString("password", ""));
    }
    String cloudToken()throws Exception{return decrypt(preferences.getString("supla_token",""));}
    private String decrypt(String saved)throws Exception{
        if (saved.isEmpty()) return "";
        String[] parts = saved.split(":", -1);
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.DECRYPT_MODE, key(), new GCMParameterSpec(128, Base64.getDecoder().decode(parts[0])));
        return new String(cipher.doFinal(Base64.getDecoder().decode(parts[1])), StandardCharsets.UTF_8);
    }
    void save(String endpoint, String username, String password) throws Exception {
        String encrypted=encrypt(password);
        if (!preferences.edit().putString("endpoint", endpoint).putString("username", username).putString("password", encrypted).commit())
            throw new java.io.IOException("Nie zapisano połączenia");
    }
    private String encrypt(String password)throws Exception{
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding"); cipher.init(Cipher.ENCRYPT_MODE, key());
        return Base64.getEncoder().encodeToString(cipher.getIV()) + ":" +
            Base64.getEncoder().encodeToString(cipher.doFinal(password.getBytes(StandardCharsets.UTF_8)));
    }
    void saveCloud(String base,String token,int device,String mode)throws Exception{
        if((!mode.equals("lan")&&device<=0)||!(mode.equals("lan")||mode.equals("auto")||mode.equals("supla")))throw new IllegalArgumentException("Nieprawidłowy tryb połączenia SUPLA");
        SharedPreferences.Editor edit=preferences.edit().putString("supla_endpoint",base).putInt("supla_device",device).putString("connection_mode",mode);
        if(!token.isEmpty())edit.putString("supla_token",encrypt(token));
        if(!edit.commit())throw new java.io.IOException("Nie zapisano ustawień SUPLA");
    }
}
