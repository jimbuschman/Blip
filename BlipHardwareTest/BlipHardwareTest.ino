/***************************************************
 * LittleFS Diagnostic
 * Lists all files on the filesystem
 ****************************************************/

#include <FS.h>
#include <LittleFS.h>

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
    Serial.printf("Listing: %s\n", dirname);
    File root = fs.open(dirname);
    if (!root) {
        Serial.println("  Failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("  Not a directory");
        return;
    }
    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            Serial.printf("  DIR: %s\n", file.name());
            if (levels) {
                listDir(fs, file.path(), levels - 1);
            }
        } else {
            Serial.printf("  FILE: %-40s  %d bytes\n", file.path(), file.size());
        }
        file = root.openNextFile();
    }
}

void setup() {
    Serial.begin(115200);
    delay(3000);
    Serial.println("=== LittleFS Diagnostic ===");

    // Try mounting WITHOUT formatting
    if (!LittleFS.begin(false)) {
        Serial.println("ERROR: LittleFS mount FAILED (no format)");
        Serial.println("This means the filesystem image is missing or incompatible.");
        Serial.println("Trying with format...");
        if (!LittleFS.begin(true)) {
            Serial.println("ERROR: LittleFS mount FAILED even with format!");
            return;
        }
        Serial.println("Mounted after format - filesystem was empty/corrupt");
    } else {
        Serial.println("LittleFS mounted OK (no format needed)");
    }

    // List all files
    Serial.println("\n--- All files on LittleFS ---");
    listDir(LittleFS, "/", 2);

    // Summary
    Serial.printf("\nTotal: %d bytes used, %d bytes total\n",
                  LittleFS.usedBytes(), LittleFS.totalBytes());
    Serial.println("\n=== Done ===");
}

void loop() {
    delay(10000);
}
