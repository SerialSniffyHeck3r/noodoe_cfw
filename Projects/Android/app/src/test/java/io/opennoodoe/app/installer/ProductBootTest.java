package io.opennoodoe.app.installer;
import org.junit.Test;
import static org.junit.Assert.*;
import io.opennoodoe.app.protocol.ByteCodec;
import java.io.IOException;

public class ProductBootTest {
    private byte[] status(int state,int flags){byte[] r=new byte[80];ByteCodec.putU32le(r,4,2);ByteCodec.putU32le(r,12,state);ByteCodec.putU32le(r,16,flags);return r;}
    @Test public void trialAndResetAreNotSuccess()throws Exception {
        assertFalse(new ProductBootSession.Status(status(3,1)).confirmed());
        assertFalse(new ProductBootSession.Status(status(4,8)).confirmed());
        assertTrue(new ProductBootSession.Status(status(4,0)).confirmed());
    }
    @Test public void resultSurvivesHealthyFallback()throws Exception {
        assertTrue(new ProductBootSession.Status(status(3,6)).rolledBack());
        assertTrue(new ProductBootSession.Status(status(4,4)).rolledBack());
    }
    @Test public void acknowledgedHistoryDoesNotBlockNewUpdate()throws Exception {
        byte[] r=status(4,0);ByteCodec.putU32le(r,36,3);
        assertFalse(new ProductBootSession.Status(r).rolledBack());
        assertTrue(new ProductBootSession.Status(r).confirmed());
    }
    @Test public void rejectsUnknownVersionOrFlags()throws Exception {
        for(byte[] r:new byte[][]{new byte[79],status(3,16),status(7,0)}){
            try{new ProductBootSession.Status(r);fail();}catch(IOException expected){}
        }
        byte[] r=status(3,1);ByteCodec.putU32le(r,4,1);
        try{new ProductBootSession.Status(r);fail();}catch(IOException expected){}
    }
}
