#!/bin/bash
# Builds MicToBluetooth.apk without Gradle, using the Ubuntu android-sdk packages:
#   apt-get install android-sdk-build-tools android-sdk-platform-23 dalvik-exchange apksigner zipalign
set -euo pipefail
cd "$(dirname "$0")"
SDK_JAR=/usr/lib/android-sdk/platforms/android-23/android.jar
OUT=build
rm -rf "$OUT" && mkdir -p "$OUT/gen" "$OUT/classes"

aapt package -f -m -J "$OUT/gen" -M AndroidManifest.xml -S res -I "$SDK_JAR" -F "$OUT/unsigned.apk"
javac -source 8 -target 8 -nowarn -Xlint:-options -bootclasspath "$SDK_JAR" -d "$OUT/classes" \
    $(find src "$OUT/gen" -name '*.java')
dalvik-exchange --dex --min-sdk-version=23 --output="$OUT/classes.dex" "$OUT/classes"
(cd "$OUT" && zip -q unsigned.apk classes.dex)
zipalign -f 4 "$OUT/unsigned.apk" "$OUT/aligned.apk"

KS=debug.keystore
if [ ! -f "$KS" ]; then
  keytool -genkeypair -keystore "$KS" -storepass android -keypass android -alias mictobt \
    -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=MicToBluetooth" >/dev/null 2>&1
fi
apksigner sign --ks "$KS" --ks-pass pass:android --key-pass pass:android \
  --out MicToBluetooth.apk "$OUT/aligned.apk"
apksigner verify MicToBluetooth.apk && echo "Built: $(pwd)/MicToBluetooth.apk"
