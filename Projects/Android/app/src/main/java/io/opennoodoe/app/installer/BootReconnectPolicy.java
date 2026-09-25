package io.opennoodoe.app.installer;
/** Retry only identity/boot-result queries after reset; never replay installation. */
public final class BootReconnectPolicy {
 public static final int ATTEMPTS=5;
 public static final long REPLY_TIMEOUT_MS=5000;
 /** Keep trying inside the existing boot window. Five quick failures while
  * Gate is flashing must not exhaust all retries before the radio starts. */
 public static int handoffDelayMs(int attempt){if(attempt<1)throw new IllegalArgumentException("Boot attempt");return attempt==1?2000:Math.min(attempt,5)*1000;}
 private static final int[] DELAYS_MS={5000,8000,12000,15000,20000};
 private BootReconnectPolicy(){}
 public static int delayMs(int attempt){if(attempt<1||attempt>ATTEMPTS)throw new IllegalArgumentException("Boot attempt");return DELAYS_MS[attempt-1];}
}
