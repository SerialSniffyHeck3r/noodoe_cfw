package io.opennoodoe.app.companion;

import android.content.*;
import android.database.Cursor;
import android.database.sqlite.*;
import java.io.*;
import java.util.*;

/** Private phone history, one bounded summary per connected IGN session segment.
 * Reconnects deliberately create a new segment: an offline ride is never reported
 * as fully imported. Every update is a SQLite transaction; no location/content is stored. */
public final class RideHistory extends SQLiteOpenHelper {
 public RideHistory(Context context){super(context,"ride-history.db",null,1);setWriteAheadLoggingEnabled(true);}
 @Override public void onCreate(SQLiteDatabase db){db.execSQL("CREATE TABLE rides(id INTEGER PRIMARY KEY,device TEXT NOT NULL,session INTEGER,start INTEGER,end INTEGER,state TEXT,moving INTEGER DEFAULT 0,stopped INTEGER DEFAULT 0,unknown INTEGER DEFAULT 0,distance REAL DEFAULT 0,max INTEGER DEFAULT 0,samples INTEGER DEFAULT 0,odo INTEGER)");}
 @Override public void onUpgrade(SQLiteDatabase db,int from,int to){throw new SQLiteException("Unsupported ride history version");}
 public long begin(String device,long session,long utc){
  SQLiteDatabase db=getWritableDatabase();db.beginTransaction();
  try{ContentValues abandoned=new ContentValues();abandoned.put("state","interrupted");db.update("rides",abandoned,"state=?",new String[]{"recording"});
   ContentValues row=new ContentValues();row.put("device",device);row.put("session",session);row.put("start",utc);row.put("end",utc);row.put("state","recording");
   long id=db.insertOrThrow("rides",null,row);db.setTransactionSuccessful();return id;
  }finally{db.endTransaction();}
 }
 public void save(long id,RideAccumulator a,RideTelemetry t,long utc){
  ContentValues row=new ContentValues();row.put("end",utc);row.put("moving",a.movingMs);row.put("stopped",a.stoppedMs);row.put("unknown",a.unknownMs);row.put("distance",a.distanceKm);row.put("max",a.maxKph);row.put("samples",a.samples);
  if(t.odometerValid)row.put("odo",t.odometer);
  if(getWritableDatabase().update("rides",row,"id=?",new String[]{Long.toString(id)})!=1)throw new SQLiteException("Ride record disappeared");
 }
 public void finish(long id,String state){if(id==0)return;ContentValues row=new ContentValues();row.put("state",state);getWritableDatabase().update("rides",row,"id=?",new String[]{Long.toString(id)});}
 public List<String> recent(){List<String> rows=new ArrayList<>();
  try(Cursor c=getReadableDatabase().rawQuery("SELECT start,distance,moving,stopped,unknown,max,state FROM rides ORDER BY id DESC LIMIT 50",null)){
   java.text.DateFormat date=new java.text.SimpleDateFormat("MM.dd HH:mm",Locale.getDefault());
   while(c.moveToNext())rows.add(date.format(new Date(c.getLong(0)))+String.format(Locale.getDefault(),io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0692," · 약 %.1f km\n이동 %d분 · 정차 %d분 · 최고 %d km/h"),c.getDouble(1),c.getLong(2)/60000,c.getLong(3)/60000,c.getLong(5))+"\n"+("ended".equals(c.getString(6))?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0693,"연결 중 시동 종료 확인"):"recording".equals(c.getString(6))?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0694,"수집 중 / 미종료 기록"):io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0695,"연결 중단 · 일부 구간만 저장"))+(c.getLong(4)>0?io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0696," · 미확인 시간 ")+c.getLong(4)/1000+io.opennoodoe.app.UiText.text(io.opennoodoe.app.R.string.companion_0191,"초"):""));
  }return rows;
 }
 public void export(OutputStream output)throws IOException {
  // Completed summaries are immutable. An in-progress row is a point-in-time
  // snapshot; export does not stop the radio or claim that the ride has ended.
  Writer out=new OutputStreamWriter(output,java.nio.charset.StandardCharsets.UTF_8);out.write("id,device,device_session,start_utc_ms,last_sample_utc_ms,state,moving_ms,stopped_ms,unknown_ms,estimated_distance_km,max_uart_kph,samples,odometer_km\n");
  try(Cursor c=getReadableDatabase().rawQuery("SELECT id,device,session,start,end,state,moving,stopped,unknown,distance,max,samples,odo FROM rides ORDER BY id",null)){
   while(c.moveToNext()){for(int i=0;i<13;i++){if(i>0)out.write(',');String v=c.isNull(i)?"":c.getString(i);out.write('"');out.write(v.replace("\"","\"\""));out.write('"');}out.write('\n');}
  }out.flush();
 }
}
