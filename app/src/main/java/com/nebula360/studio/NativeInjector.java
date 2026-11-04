package com.nebula360.studio;

public final class NativeInjector {
    static {
        System.loadLibrary("nebula360");
    }

    private NativeInjector() {}

    public static native String inject(String inputPath, String outputPath, int stereoMode);
}
