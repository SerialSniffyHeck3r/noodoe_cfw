package io.opennoodoe.app.installer;

import io.opennoodoe.app.protocol.ByteCodec;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;

/** Independent host FAT12 audit over a disk snapshot. Never loads the128MiB NOR into Android RAM. */
public final class BootstrapFatPlan implements Closeable {
    public static final String[] NAMES={"NOODOE.RSC","CFWCFG.DAT","CFWRIDE.DAT","CFWPIC.DAT","CFWREC.DAT","CFWA.DAT","CFWB.DAT","CFWBOOT.DAT","CFWLOG.DAT"};
    private static final String[] SHORT={"NOODOE  RSC","CFWCFG  DAT","CFWRIDE DAT","CFWPIC  DAT","CFWREC  DAT","CFWA    DAT","CFWB    DAT","CFWBOOT DAT","CFWLOG  DAT"};
    public interface ReadAccess extends Closeable {byte[] read(long offset,int bytes)throws IOException;}
    private final ReadAccess reader;
    private static ReadAccess full(File snapshot)throws IOException{
        RandomAccessFile file=new RandomAccessFile(snapshot,"r");
        if(file.length()!=0x8000000L){file.close();throw new IOException("Complete NOR snapshot required");}
        return new ReadAccess(){public byte[] read(long off,int n)throws IOException{byte[] b=new byte[n];file.seek(off);file.readFully(b);return b;}public void close()throws IOException{file.close();}};
    }
    public final byte[] metadata;
    private final boolean[] owned=new boolean[4080];
    private final Map<String,Entry> entries=new HashMap<>();
    private static final class Entry {int[] chain;long size;int attr;Entry(int[] chain,long size,int attr){this.chain=chain;this.size=size;this.attr=attr;}}
    public BootstrapFatPlan(File snapshot)throws IOException {this(full(snapshot));}
    public BootstrapFatPlan(ReadAccess access)throws IOException {
        reader=access;
        try {
            metadata=read(0,0x9000,true);
            require(u16(metadata,11)==4096&&(metadata[13]&255)==8&&u16(metadata,14)==1&&metadata[16]==2
                    &&u16(metadata,17)==512&&u16(metadata,22)==2&&u32(metadata,32)==0x7f80,"Unsupported FAT geometry");
            require(Arrays.equals(Arrays.copyOfRange(metadata,0x1000,0x3000),Arrays.copyOfRange(metadata,0x3000,0x5000)),"FAT mirrors differ");
            require((metadata[510]&255)==0x55&&(metadata[511]&255)==0xaa&&fat(0)==(0xf00|(metadata[21]&255))&&fat(1)>=0xff8,"Invalid FAT reserved entries");
            walk(Arrays.copyOfRange(metadata,0x5000,0x9000),"",0);
            for(int c=2;c<4080;c++)require(fat(c)==0||fat(c)==0xff7||owned[c],"Orphan allocation; no automatic repair");
        } catch(IOException|RuntimeException e){reader.close();throw e;}
    }
    public byte[] read(long offset,int n,boolean logical)throws IOException {
        byte[] data=reader.read(offset,n);return logical?swap(data):data;
    }
    private int fat(int c){int v=u16(metadata,0x1000+c+c/2);return (c&1)!=0?v>>>4:v&0xfff;}
    public static long address(int c){return 0x9000L+(c-2)*32768L;}
    private void walk(byte[] data,String path,int depth)throws IOException {
        require(depth<=16,"Directory depth exceeded");
        for(int off=0;off<data.length;off+=32) {
            int start=data[off]&255,attr=data[off+11]&255;if(start==0)break;
            if(start==0xe5||attr==15||(attr&8)!=0||start==46)continue;
            String base=new String(data,off,8,StandardCharsets.US_ASCII).trim(),ext=new String(data,off+8,3,StandardCharsets.US_ASCII).trim();
            String name=path+base+(ext.isEmpty()?"":"."+ext);
            require(!entries.containsKey(name)&&u16(data,off+20)==0,"Duplicate filename/high cluster");
            int c=u16(data,off+26);long size=u32(data,off+28);List<Integer> chain=new ArrayList<>();
            if(c!=0)while(c<0xff8){require(c>=2&&c<4080&&!owned[c],"Crosslinked/broken FAT chain");owned[c]=true;chain.add(c);c=fat(c);}
            require(((attr&16)!=0&&!chain.isEmpty())||size<=chain.size()*32768L,"Truncated file");
            int[] clusters=new int[chain.size()];for(int i=0;i<clusters.length;i++)clusters[i]=chain.get(i);
            entries.put(name,new Entry(clusters,size,attr));
            if((attr&16)!=0){require(chain.size()<=32,"Unexpected oversized directory");ByteArrayOutputStream directory=new ByteArrayOutputStream();
                for(int cluster:clusters)directory.write(read(address(cluster),32768,true));walk(directory.toByteArray(),name+"/",depth+1);}
        }
    }
    public byte[] photo(int slot)throws IOException {
        String name="WALL"+slot+".JPG";
        if(!entries.containsKey(name)){List<String> matches=new ArrayList<>();for(String key:entries.keySet())if(key.startsWith("ALBUM/"+slot+"/")&&key.endsWith(".JPG"))matches.add(key);
            require(matches.size()==1,"Photo source ambiguous for slot "+slot);name=matches.get(0);}
        Entry e=entries.get(name);require(e.size>0&&e.size<=131072,"Photo size unsupported");
        ByteArrayOutputStream data=new ByteArrayOutputStream();for(int c:e.chain)data.write(read(address(c),32768,true));return Arrays.copyOf(data.toByteArray(),(int)e.size);
    }
    public void rejectLegacy()throws IOException {
        for(int i=0;i<16;i++){byte[] record=read(0x7f70000L+i*4096,4096,false);
            require(u32(record,0)!=0x4e564d31L||u32(record,4092)!=0x434d5431L,"Legacy NVM candidate present; use audited migration client");}
    }
    public static void validateJpegHeader(byte[] bytes)throws IOException {
        require(bytes.length>=4&&(bytes[0]&255)==255&&(bytes[1]&255)==216
                &&(bytes[bytes.length-2]&255)==255&&(bytes[bytes.length-1]&255)==217,"Complete JPEG required");
        boolean baseline=false;int position=2;
        while(position+3<bytes.length){require((bytes[position++]&255)==255,"JPEG marker boundary");while(position<bytes.length&&(bytes[position]&255)==255)position++;
            require(position<bytes.length,"Truncated JPEG marker");int marker=bytes[position++]&255;if(marker==0xda)break;
            int n=((bytes[position]&255)<<8)|(bytes[position+1]&255);require(n>=2&&position+n<=bytes.length,"JPEG segment bounds");
            if(marker>=0xc0&&marker<=0xcf&&marker!=0xc4&&marker!=0xc8&&marker!=0xcc){require(marker==0xc0&&n>=8,"Baseline JPEG only");
                int height=((bytes[position+3]&255)<<8)|(bytes[position+4]&255),width=((bytes[position+5]&255)<<8)|(bytes[position+6]&255);
                require(width>0&&width<=480&&height>0&&height<=480,"JPEG dimensions");baseline=true;}
            position+=n;
        }
        require(baseline,"JPEG baseline frame missing");
    }
    /** Returns the same first-fit extent selected independently by Bootstrap firmware. */
    public boolean hasContainer(int kind){return entries.containsKey(NAMES[kind]);}
    public int existingContainers(){int n=0;for(String name:NAMES)if(entries.containsKey(name))n++;return n;}
    /** Read-only reuse requires all nine exact, owned, contiguous extents. */
    public long existingAddress(int kind,int size)throws IOException {
        Entry e=entries.get(NAMES[kind]);
        require(e!=null&&(e.attr&0x18)==0&&e.size==size&&e.chain.length==size/32768&&size%32768==0,"Existing container shape differs: "+NAMES[kind]);
        for(int i=1;i<e.chain.length;i++)require(e.chain[i]==e.chain[0]+i,"Fragmented CFW container: "+NAMES[kind]);
        long at=address(e.chain[0]);require(at+size<=0x7f70000L,"CFW container crosses reserved area");return at;
    }
    public long allocate(int kind,int size)throws IOException {
        require(kind>=0&&kind<NAMES.length&&!entries.containsKey(NAMES[kind]),"Existing CFW filename; never replace");
        require(size%32768==0,"Cluster alignment");int count=size/32768,first=-1,root=-1;
        for(int c=2;c+count<=4080;c++){if(address(c)+size>0x7f70000L)break;boolean free=true;for(int j=0;j<count;j++)if(fat(c+j)!=0||owned[c+j]){free=false;break;}if(free){first=c;break;}}
        for(int i=0;i<512;i++){int ch=metadata[0x5000+i*32]&255;if(ch==0||ch==0xe5){root=i;break;}}
        require(first>=0&&root>=0,"No safe contiguous space/root entry");
        for(int i=0;i<count;i++){int c=first+i,n=i+1==count?0xfff:c+1;for(int base:new int[]{0x1000,0x3000}){int offset=base+c+c/2,v=u16(metadata,offset);ByteCodec.putU16le(metadata,offset,(c&1)!=0?(v&15)|(n<<4):(v&0xf000)|n);}}
        int offset=0x5000+root*32;Arrays.fill(metadata,offset,offset+32,(byte)0);byte[] shortName=SHORT[kind].getBytes(StandardCharsets.US_ASCII);
        System.arraycopy(shortName,0,metadata,offset,11);metadata[offset+11]=0x20;ByteCodec.putU16le(metadata,offset+26,first);ByteCodec.putU32le(metadata,offset+28,size);
        return address(first);
    }
    public static byte[] record(int purpose,long[] uid,byte[] payload,int generation) {
        byte[] data=filled(4096);long[] header={0x314a4643,1,purpose,generation,payload.length,uid[0],uid[1],uid[2]};
        for(int i=0;i<header.length;i++)ByteCodec.putU32le(data,i*4,header[i]);System.arraycopy(payload,0,data,64,payload.length);
        ByteCodec.putU32le(data,4088,NdcpSession.crc(data,4088));ByteCodec.putU32le(data,4092,0x31544d43);return data;
    }
    public static byte[] filled(int size){byte[] data=new byte[size];Arrays.fill(data,(byte)255);return data;}
    public static byte[] swap(byte[] source){byte[] out=source.clone();for(int i=0;i+1<out.length;i+=2){out[i]=source[i+1];out[i+1]=source[i];}return out;}
    private static int u16(byte[] b,int at){return ByteCodec.u16le(b,at);}
    private static long u32(byte[] b,int at){return ByteCodec.u32le(b,at);}
    static void require(boolean condition,String message)throws IOException{if(!condition)throw new IOException(message);}
    @Override public void close()throws IOException{reader.close();}
}
