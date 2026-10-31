#!/bin/sh
# Get an Apple Development certificate and a provisioning profile for an iPad app.
#
#   ios/provision.sh CONFIG TEAM_ID [OUT_PROFILE]
#
# The engine is built without Xcode's IDE, so this builds a throwaway stub app with the
# config's bundle id and automatic signing: Xcode (signed in to the Apple ID of TEAM_ID)
# creates the certificate in the login keychain, registers connected devices, and makes the
# profile, copied to OUT_PROFILE (default ios-deps/NAME.mobileprovision). A personal (free)
# team needs the iPad connected and trusted; its profiles last 7 days.
# Then: ios/make_app.sh CONFIG BUILD_DIR OUT_DIR "Apple Development" PROFILE
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
[ $# -ge 2 ] || { sed -n '2,12p' "$0"; exit 1; }
cfg="$1"; team="$2"
name="$(python3 -I "$here/app.py" get "$cfg" name)"
bundle_id="$(python3 -I "$here/app.py" get "$cfg" bundle_id)"
out="${3:-${XSYS4_IOS_DEPS:-$here/../ios-deps}/$name.mobileprovision}"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/Stub.xcodeproj" "$tmp/Stub"
printf 'int main(void) { return 0; }\n' > "$tmp/Stub/main.c"
cat > "$tmp/Stub.xcodeproj/project.pbxproj" <<EOT
// !\$*UTF8*\$!
{
	archiveVersion = 1;
	classes = {};
	objectVersion = 56;
	objects = {
		A0000000000000000000001 = {isa = PBXProject; buildConfigurationList = A0000000000000000000002; compatibilityVersion = "Xcode 14.0"; mainGroup = A0000000000000000000003; productRefGroup = A0000000000000000000003; projectDirPath = ""; projectRoot = ""; targets = (A0000000000000000000010); attributes = {TargetAttributes = {A0000000000000000000010 = {ProvisioningStyle = Automatic;};};};};
		A0000000000000000000002 = {isa = XCConfigurationList; buildConfigurations = (A0000000000000000000004); defaultConfigurationName = Release;};
		A0000000000000000000004 = {isa = XCBuildConfiguration; name = Release; buildSettings = {SDKROOT = iphoneos; IPHONEOS_DEPLOYMENT_TARGET = 17.0;};};
		A0000000000000000000003 = {isa = PBXGroup; children = (A0000000000000000000005, A0000000000000000000006); sourceTree = "<group>";};
		A0000000000000000000005 = {isa = PBXFileReference; lastKnownFileType = sourcecode.c.c; path = Stub/main.c; sourceTree = "<group>";};
		A0000000000000000000006 = {isa = PBXFileReference; explicitFileType = wrapper.application; path = Stub.app; sourceTree = BUILT_PRODUCTS_DIR; includeInIndex = 0;};
		A0000000000000000000007 = {isa = PBXBuildFile; fileRef = A0000000000000000000005;};
		A0000000000000000000008 = {isa = PBXSourcesBuildPhase; files = (A0000000000000000000007);};
		A0000000000000000000010 = {isa = PBXNativeTarget; name = Stub; productName = Stub; productType = "com.apple.product-type.application"; productReference = A0000000000000000000006; buildConfigurationList = A0000000000000000000011; buildPhases = (A0000000000000000000008); buildRules = (); dependencies = ();};
		A0000000000000000000011 = {isa = XCConfigurationList; buildConfigurations = (A0000000000000000000012); defaultConfigurationName = Release;};
		A0000000000000000000012 = {isa = XCBuildConfiguration; name = Release; buildSettings = {
			PRODUCT_NAME = Stub; PRODUCT_BUNDLE_IDENTIFIER = $bundle_id; TARGETED_DEVICE_FAMILY = 2;
			GENERATE_INFOPLIST_FILE = YES; INFOPLIST_KEY_UILaunchScreen_Generation = YES;
			CODE_SIGN_STYLE = Automatic; DEVELOPMENT_TEAM = $team; CODE_SIGN_IDENTITY = "Apple Development";
			SDKROOT = iphoneos; IPHONEOS_DEPLOYMENT_TARGET = 17.0;};};
	};
	rootObject = A0000000000000000000001;
}
EOT
xcodebuild -project "$tmp/Stub.xcodeproj" -target Stub -configuration Release \
	-allowProvisioningUpdates -allowProvisioningDeviceRegistration \
	SYMROOT="$tmp/build" OBJROOT="$tmp/obj" build -quiet
mkdir -p "$(dirname "$out")"
cp "$tmp/build/Release-iphoneos/Stub.app/embedded.mobileprovision" "$out"
security cms -D -i "$out" > "$tmp/p.plist"
echo "$out: $(plutil -extract Name raw "$tmp/p.plist"), expires $(plutil -extract ExpirationDate raw "$tmp/p.plist"), $(plutil -extract ProvisionedDevices raw "$tmp/p.plist" 2>/dev/null || echo 0) device(s)"
