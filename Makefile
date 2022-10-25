macos: src/macos.mm
	@mkdir -p build/macOS.app/Contents/MacOS
	@mkdir -p build/macOS.app/Contents/Resources
	@cp src/assets/*.metal build/macOS.app/Contents/Resources/
	clang++ --target=x86_64-apple-darwin -fmodules -fcxx-modules --std=c++17 -O3 -Wall -Werror -pedantic -Wno-unused-parameter -mmacosx-version-min=10.10 -framework AudioUnit src/macos.mm -o ./build/macOS.app/Contents/MacOS/macOS-intel
	clang++ --target=aarch64-apple-darwin -fmodules -fcxx-modules --std=c++17 -O3 -Wall -Werror -pedantic -Wno-unused-parameter -mmacosx-version-min=10.10 -framework AudioUnit src/macos.mm -o ./build/macOS.app/Contents/MacOS/macOS-apple
	@lipo ./build/macOS.app/Contents/MacOS/macOS-intel ./build/macOS.app/Contents/MacOS/macOS-apple -create -output ./build/macOS.app/Contents/MacOS/macOS
	@rm ./build/macOS.app/Contents/MacOS/macOS-intel ./build/macOS.app/Contents/MacOS/macOS-apple
	@codesign --force --deep --sign - build/macOS.app/Contents/MacOS/macOS

ios-sim: src/ios.mm
	@mkdir -p build/iOS-sim.app/
	clang++ -isysroot $(xcrun --sdk iphoneos --show-sdk-path) --target=aarch64-apple-ios -fmodules -fcxx-modules --std=c++17 -O3 -Wall -Werror -pedantic -Wno-unused-parameter -mios-simulator-version-min=13.0 src/ios.mm -o build/iOS-sim.app/iOS-Sim
	@cp src/assets/*.metal build/iOS-sim.app/

ios: src/ios.mm
	@mkdir -p build/iOS.app/
	clang++ -isysroot $(xcrun --sdk iphonesimulator --show-sdk-path) --target=aarch64-apple-ios -fmodules -fcxx-modules --std=c++17 -O3 -Wall -Werror -pedantic -Wno-unused-parameter src/ios.mm -o build/iOS.app/iOS
	@cp src/assets/*.metal build/iOS.app/

win32: src/win32.cpp
	c++ -Wall /std:c++20 -Luser32 -Ld3d11 -Ld3dcompiler -Ldxguid -Ldsound -Lxinput src/win32.cpp -o build/win32.exe

android: src/android.cpp
	clang++ -O3 -Wall -Werror -Wno-unused-parameter -I$(ANDROID_NDK_HOME)/sources/android/native_app_glue $(ANDROID_NDK_HOME)/sources/android/native_app_glue/android_native_app_glue.c src/android.cpp -o build/android.so

x11: src/x11.cpp
	clang++ --std=c++17 -O3 -Wall -Werror -Wl,-s -Wno-unused-parameter -Wno-unused-but-set-variable -LX11 -LEGL -LGL -LGLEW -Lasound src/x11.cpp -o build/x11
