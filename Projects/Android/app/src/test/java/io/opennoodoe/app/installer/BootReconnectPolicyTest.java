package io.opennoodoe.app.installer;
import org.junit.Test;
import static org.junit.Assert.*;
public class BootReconnectPolicyTest {
 @Test public void fiveAttemptsHaveBoundedBackoff(){int total=0;for(int i=1;i<=BootReconnectPolicy.ATTEMPTS;i++)total+=BootReconnectPolicy.delayMs(i);assertEquals(5,BootReconnectPolicy.ATTEMPTS);assertEquals(60000,total);}
 @Test(expected=IllegalArgumentException.class)public void sixthAttemptRejected(){BootReconnectPolicy.delayMs(6);}
}
