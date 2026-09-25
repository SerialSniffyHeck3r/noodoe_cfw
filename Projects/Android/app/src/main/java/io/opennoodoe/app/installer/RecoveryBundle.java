package io.opennoodoe.app.installer;

import io.opennoodoe.app.protocol.ByteCodec;
import io.opennoodoe.app.protocol.DeviceInfo;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.*;
import java.util.zip.*;

/** Explicit local import, never a stock full dump or an unchecked filename-selected image. */
public final class RecoveryBundle {
    public static final int APP_BASE = 0x08010000, APP_BYTES = 0x70000;
    private static final int MAX_ARCHIVE = 4 * 1024 * 1024;
    public final String sha256;
    public final Properties manifest;
    private final Map<String, byte[]> entries;
    private RecoveryBundle(byte[] zip, Properties properties, Map<String, byte[]> files) {
        sha256 = sha(zip); manifest = properties; entries = files;
    }
    public static RecoveryBundle read(InputStream input) throws Exception {
        byte[] archive = bounded(input, MAX_ARCHIVE);
        Map<String, byte[]> files = new HashMap<>();
        int total = 0;
        try (ZipInputStream zip = new ZipInputStream(new ByteArrayInputStream(archive))) {
            ZipEntry item;
            while ((item = zip.getNextEntry()) != null) {
                String name = item.getName();
                if (item.isDirectory() || !name.matches("[A-Za-z0-9_.-]+") || name.contains(".."))
                    throw new IOException("Bundle entries must be flat regular files");
                byte[] data = bounded(zip, MAX_ARCHIVE);
                total += data.length;
                if (total > MAX_ARCHIVE || files.size() >= 12 || files.put(name, data) != null)
                    throw new IOException("Duplicate or oversized bundle entry");
            }
        }
        byte[] raw = files.get("manifest.properties");
        if (raw == null || raw.length > 16384) throw new IOException("Missing bounded manifest");
        Properties p = new Properties();
        for (String line : new String(raw, StandardCharsets.UTF_8).split("\\r?\\n")) {
            if (line.isEmpty() || line.startsWith("#")) continue;
            int separator=line.indexOf('=');
            if (separator<1 || line.indexOf('\\')>=0) throw new IOException("Use canonical key=value lines without escapes");
            String key=line.substring(0,separator), value=line.substring(separator+1);
            if (!key.equals(key.trim()) || !value.equals(value.trim()) || p.containsKey(key))
                throw new IOException("Duplicate or noncanonical manifest key/value");
            p.setProperty(key,value);
        }
        if (!"NOODOE_INSTALLER_2".equals(p.getProperty("format"))
                || number(p, "app.base") != APP_BASE || number(p, "app.bytes") != APP_BYTES)
            throw new IOException("Unsupported recovery bundle layout");
        RecoveryBundle result = new RecoveryBundle(archive, p, files);
        if(number(p,"layout.version")!=2)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0568,"독립 리커버리가 포함된 설치 파일이 필요해요."));
        for (String key : new String[]{"target.hardware", "target.boot.major", "target.boot.minor",
                "target.stock.major", "target.stock.minor"}) checkedU16(number(p, key));
        required(p, "target.model");
        required(p, "target.pcba");
        if(!Arrays.asList("observed","bench-only").contains(required(p,"target.scope")))throw new IOException("Unknown target evidence scope");
        String binding=p.getProperty("target.boot.binding","pinned");
        if(!Arrays.asList("pinned","pinned-capture","device-capture").contains(binding))throw new IOException("Unknown resident binding");
        if(binding.equals("device-capture")){
            if(!required(p,"target.boot.sha256").equals("capture-on-bootstrap")||number(p,"target.boot.major")!=0||number(p,"target.boot.minor")!=15||
               !required(p,"target.pcba").equals("SR0701")||!required(p,"target.model").equals("SAA1AA(KR)")||number(p,"target.hardware")!=0)
                throw new IOException("Unsupported first-use target profile");
        }else if (!required(p,"target.boot.sha256").matches("[0-9a-f]{64}")) throw new IOException("BL code hash required");
        Set<String> expected = new HashSet<>(Collections.singleton("manifest.properties"));
        for (String role : new String[]{"bootstrap", "cfw", "stock"}) {
            String name = required(p, role + ".file");
            if (!name.endsWith(".bin") || !expected.add(name)) throw new IOException("Distinct APP files required");
            byte[] image = result.image(role);
            validateApp(image);
            checkedU16(number(p, role + ".major")); checkedU16(number(p, role + ".minor"));
        }
        if (p.containsKey("uninstall.file")) {
            if (!expected.add(required(p,"uninstall.file"))) throw new IOException("Duplicate uninstall role");
            validateUninstall(result.image("uninstall"));
            if(number(p,"uninstall.version")!=7)throw new IOException("Unsupported uninstall transport version");
        }
        if(p.containsKey("diagnostic.file")){
            if(!expected.add(required(p,"diagnostic.file")))throw new IOException("Duplicate diagnostic role");
            validateDiagnostic(result.image("diagnostic"));
            if(number(p,"diagnostic.version")!=1)throw new IOException("Unknown diagnostic contract");
            byte[] gate=result.paddedImage("cfw");
            if(ByteCodec.u32le(gate,0x200)!=0x31544647L||ByteCodec.u32le(gate,0x204)!=1||
               (ByteCodec.u32le(gate,0x208)&1)==0||ByteCodec.u32le(gate,0x20c)!=(~ByteCodec.u32le(gate,0x208)&0xffffffffL))
                throw new IOException("Diagnostic requires its matching typed RecoveryGate");
        }
        if (p.containsKey("resources.file")) {
            if (!expected.add(required(p, "resources.file"))) throw new IOException("Duplicate role entry");
            result.image("resources");
        }
        if (!files.keySet().equals(expected)) {
            Set<String> extra=new TreeSet<>(files.keySet());extra.removeAll(expected);
            Set<String> missing=new TreeSet<>(expected);missing.removeAll(files.keySet());
            throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0569,"ZIP 파일 목록이 manifest와 맞지 않아요. 최신 APK와 함께 배포한 ZIP을 선택하세요. 추가=")+extra+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0570,", 누락=")+missing);
        }
        if (number(p, "stock.major") != number(p, "target.stock.major")
                || number(p, "stock.minor") != number(p, "target.stock.minor"))
            throw new IOException("Recovery image version differs from target stock version");
        GateContainers.product(result.paddedImage("cfw"));
        if(!p.containsKey("resources.file"))throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0571,"설치에 필요한 자산 파일이 빠졌어요."));
        return result;
    }
    /** Standalone vectors + erased S4 journal + S5-S7 executable; never a BL image. */
    public static void validateUninstall(byte[] b)throws IOException {
        validateApp(b);
        if(b.length!=APP_BYTES||ByteCodec.u32le(b,4)<0x08020001L||
           ByteCodec.u32le(b,0x200)!=0x31424e55L||ByteCodec.u32le(b,0x204)!=1||
           ByteCodec.u32le(b,0x208)!=APP_BASE||ByteCodec.u32le(b,0x20c)!=APP_BYTES||
           ByteCodec.u32le(b,0x210)!=0x08011000L||ByteCodec.u32le(b,0x214)!=0xf000||
           ByteCodec.u32le(b,0x218)!=0x00100005L)throw new IOException("Uninstall image layout mismatch");
        for(int i=0x1000;i<0x10000;i++)if(b[i]!=(byte)255)throw new IOException("Uninstall work journal is not empty");
    }
    /** Temporary role3 cannot masquerade as Product, use external fonts, or replace Gate. */
    public static void validateDiagnostic(byte[] b)throws IOException {
        if(b.length!=0x60000||ByteCodec.u32le(b,0)!=0x2002ff00L||
           (ByteCodec.u32le(b,4)&1)==0||ByteCodec.u32le(b,4)<0x08020001L||ByteCodec.u32le(b,4)>=0x08080000L||
           ByteCodec.u32le(b,0x200)!=0x51534352L||ByteCodec.u32le(b,0x204)!=1||ByteCodec.u32le(b,0x208)!=0||
           ByteCodec.u32le(b,0x230)!=0x3250554eL||ByteCodec.u32le(b,0x234)!=2||
           ByteCodec.u32le(b,0x238)!=3||ByteCodec.u32le(b,0x23c)!=0x60000)throw new IOException("Diagnostic image contract mismatch");
        for(int i=0x20c;i<0x22c;i++)if(b[i]!=0)throw new IOException("Diagnostic cannot require Product assets");
    }
    public byte[] image(String role) throws IOException {
        byte[] data = entries.get(required(manifest, role + ".file"));
        String expected = required(manifest, role + ".sha256");
        if (data == null || !expected.matches("[0-9a-f]{64}") || !sha(data).equals(expected))
            throw new IOException("SHA-256 mismatch: " + role);
        return data.clone();
    }
    public byte[] paddedImage(String role) throws IOException {
        byte[] original = image(role), data = new byte[APP_BYTES];
        Arrays.fill(data, (byte)0xff); System.arraycopy(original, 0, data, 0, original.length);
        return data;
    }
    public void checkStock(DeviceInfo info) throws IOException {
        requireInstallable();
        if (!info.modern || info.status != 0 || info.hardwareVersion != number(manifest, "target.hardware")
                || info.bootMajor != number(manifest, "target.boot.major")
                || info.bootMinor != number(manifest, "target.boot.minor")
                || info.firmwareMajor != number(manifest, "target.stock.major")
                || info.firmwareMinor != number(manifest, "target.stock.minor")
                || !info.model.equals(required(manifest, "target.model"))
                || !info.pcba.equals(required(manifest, "target.pcba")))
            throw new IOException("Actual stock device does not match the imported bundle");
    }
    public void requireInstallable() throws IOException {
        if(!"observed".equals(required(manifest,"target.scope")))throw new IOException("Bench-only reconstructed bundle: wireless installation is disabled");
        String deployment=manifest.getProperty("target.deployment");
        boolean diagnostic="bootstrap-diagnostics".equals(deployment)&&Arrays.asList("pinned-capture","device-capture").contains(manifest.getProperty("target.boot.binding"));
        if(!"no-swd-validated".equals(deployment)&&!diagnostic)throw new IOException(io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0572,"이 패키지는 설치용으로 준비되지 않았어요."));
    }
    public static void validateApp(byte[] data) throws IOException {
        if (data.length < 0x1ac || data.length > APP_BYTES) throw new IOException("APP-only image required");
        long sp = ByteCodec.u32le(data, 0), pc = ByteCodec.u32le(data, 4);
        if (sp <= 0x20000000L || sp > 0x20030000L || (sp & 7) != 0
                || (pc & 1) == 0 || pc - 1 < APP_BASE || pc - 1 >= APP_BASE + data.length)
            throw new IOException("Invalid APP vectors/address");
    }
    public static long number(Properties p, String key) throws IOException {
        try { return Long.decode(required(p, key)); }
        catch (NumberFormatException e) { throw new IOException("Invalid number: " + key, e); }
    }
    private static void checkedU16(long v) throws IOException {
        if (v < 0 || v > 65535) throw new IOException("Version/identity outside uint16");
    }
    public static String required(Properties p, String key) throws IOException {
        String value = p.getProperty(key);
        if (value == null || value.isEmpty()) throw new IOException("Missing " + key);
        return value;
    }
    public static String sha(byte[] data) {
        try {
            StringBuilder s = new StringBuilder();
            for (byte b : MessageDigest.getInstance("SHA-256").digest(data)) s.append(String.format(Locale.ROOT,"%02x",b & 255));
            return s.toString();
        } catch (Exception e) { throw new IllegalStateException(e); }
    }
    public static byte[] bounded(InputStream input, int limit) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream(); byte[] buffer = new byte[8192]; int n;
        while ((n = input.read(buffer)) != -1) {
            if (out.size() + n > limit) throw new IOException("Input exceeds limit");
            out.write(buffer, 0, n);
        }
        return out.toByteArray();
    }
}
