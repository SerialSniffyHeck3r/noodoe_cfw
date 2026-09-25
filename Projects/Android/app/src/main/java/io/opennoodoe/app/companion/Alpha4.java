package io.opennoodoe.app.companion;
/** Native EVE L4 / LVGL A4: left pixel in the high nibble, byte-aligned rows. */
public final class Alpha4 {
 private Alpha4(){}
 public static byte[] pack(int[] argb){if((argb.length&1)!=0)throw new IllegalArgumentException("even pixel count required");byte[] out=new byte[argb.length/2];for(int i=0;i<argb.length;i+=2){int a=((argb[i]>>>24)*15+127)/255,b=((argb[i+1]>>>24)*15+127)/255;out[i/2]=(byte)((a<<4)|b);}return out;}
}
