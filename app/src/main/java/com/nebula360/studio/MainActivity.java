package com.nebula360.studio;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.media.MediaMetadataRetriever;
import android.media.MediaPlayer;
import android.view.Gravity;
import android.view.View;
import android.widget.*;

import java.io.*;
import java.util.ArrayList;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends Activity implements SpherePreview.SurfaceReadyListener {
    private static final int REQUEST_VIDEO = 1001, REQUEST_OUTPUT = 1002;
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final Handler main = new Handler(Looper.getMainLooper());
    private final ArrayList<Clip> clips = new ArrayList<>();
    private Uri surfaceUri;
    private String sourcePath;
    private SpherePreview preview;
    private TextView status, details, timecode, inspectorTitle, inspectorBody, mediaCount;
    private Spinner stereoSpinner;
    private Button exportButton;
    private TimelineView timeline;
    private MediaPlayer player;
    private android.view.Surface videoSurface;
    private android.graphics.SurfaceTexture surfaceTexture;
    private int selected = -1;
    private int panel = 0;

    private static class Clip {
        String name, path;
        Uri uri;
        long durationMs;
        int color;
        Clip(String n, String p, Uri u, long d, int c) { name=n; path=p; uri=u; durationMs=d; color=c; }
    }

    @Override public void onCreate(Bundle state) { super.onCreate(state); buildWorkspace(); }

    private void buildWorkspace() {
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setBackgroundColor(0xff080b16);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(12), dp(10), dp(12), dp(22));
        scroll.addView(root);

        // Compact professional top bar
        LinearLayout top = row();
        LinearLayout brand = col();
        TextView logo = txt("NEBULA 360", 19, Color.WHITE, true);
        TextView project = txt("PRO WORKSPACE  /  Untitled Project", 9, 0xff8291b2, true);
        brand.addView(logo); brand.addView(project);
        top.addView(brand, new LinearLayout.LayoutParams(0, -2, 1));
        Button save = smallButton("SALVAR", false); save.setOnClickListener(v -> toast("Projeto temporário nesta versão."));
        top.addView(save);
        Button exportTop = smallButton("EXPORTAR  ↗", true); exportTop.setOnClickListener(v -> chooseOutput());
        LinearLayout.LayoutParams ep = new LinearLayout.LayoutParams(-2, dp(40)); ep.leftMargin=dp(6); top.addView(exportTop, ep);
        root.addView(top);
        addDivider(root, 14);

        // Workspace tabs
        LinearLayout tabs = row();
        String[] tabNames = {"EDITAR", "COR", "ÁUDIO", "360°"};
        for (int i=0;i<tabNames.length;i++) {
            final int index=i;
            Button b = smallButton(tabNames[i], i==0);
            b.setOnClickListener(v -> { panel=index; updatePanel(); });
            LinearLayout.LayoutParams bp = new LinearLayout.LayoutParams(0, dp(36), 1); bp.rightMargin=dp(4); tabs.addView(b,bp);
        }
        root.addView(tabs);

        // Viewer panel
        LinearLayout viewerCard = card();
        LinearLayout viewerHead = row();
        viewerHead.addView(txt("PROGRAM MONITOR", 10, 0xff9aa9ca, true), new LinearLayout.LayoutParams(0,-2,1));
        viewerHead.addView(txt("EQUIRECTANGULAR  •  360°", 9, 0xff25d8f5, true));
        viewerCard.addView(viewerHead);
        preview = new SpherePreview(this);
        preview.setSurfaceReadyListener(this);
        LinearLayout.LayoutParams vp = new LinearLayout.LayoutParams(-1, dp(230)); vp.topMargin=dp(10);
        viewerCard.addView(preview,vp);
        TextView gesture = txt("TOQUE E ARRASTE PARA OLHAR AO REDOR   ·   PINÇA PARA ZOOM", 9, 0xff7182a8, true);
        gesture.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams gp = new LinearLayout.LayoutParams(-1,-2); gp.topMargin=dp(8); viewerCard.addView(gesture,gp);
        root.addView(viewerCard);

        // Transport controls
        LinearLayout transport = row(); transport.setGravity(Gravity.CENTER_VERTICAL);
        timecode = txt("00:00:00:00", 14, Color.WHITE, true);
        transport.addView(timecode,new LinearLayout.LayoutParams(0,-2,1));
        Button back = transportButton("|◀"); back.setOnClickListener(v -> seekStart()); transport.addView(back);
        Button play = transportButton("▶"); play.setOnClickListener(v -> togglePlayback()); transport.addView(play);
        Button next = transportButton("▶|"); next.setOnClickListener(v -> seekEnd()); transport.addView(next);
        Button fit = transportButton("FIT"); fit.setOnClickListener(v -> { if (preview!=null) preview.setVideoUri(surfaceUri); }); transport.addView(fit);
        root.addView(transport);
        addDivider(root, 10);

        // Media bin + tool shelf
        LinearLayout sectionHead = row();
        sectionHead.addView(txt("PROJECT MEDIA", 10, 0xff9aa9ca, true),new LinearLayout.LayoutParams(0,-2,1));
        mediaCount = txt("0 CLIPS", 9, 0xff25d8f5, true); sectionHead.addView(mediaCount);
        root.addView(sectionHead);
        LinearLayout mediaTools = row();
        Button importBtn = smallButton("＋ IMPORTAR", true); importBtn.setOnClickListener(v -> chooseVideo());
        mediaTools.addView(importBtn,new LinearLayout.LayoutParams(0,dp(40),1));
        Button addBtn = smallButton("ADICIONAR À TIMELINE", false); addBtn.setOnClickListener(v -> { if(selected>=0) toast("Clipe já está na timeline."); else chooseVideo(); });
        LinearLayout.LayoutParams abp = new LinearLayout.LayoutParams(0,dp(40),1); abp.leftMargin=dp(6); mediaTools.addView(addBtn,abp);
        root.addView(mediaTools);
        details = txt("Importe um MP4 360° para começar. Seus arquivos aparecem aqui.", 10, 0xff7f8eae, false);
        LinearLayout.LayoutParams detp = new LinearLayout.LayoutParams(-1,-2); detp.topMargin=dp(8); root.addView(details,detp);

        // Timeline toolbar
        addDivider(root, 14);
        LinearLayout timelineHead = row();
        timelineHead.addView(txt("TIMELINE", 11, Color.WHITE, true),new LinearLayout.LayoutParams(0,-2,1));
        Button zoomOut=smallButton("−",false), zoomIn=smallButton("＋",false);
        zoomOut.setOnClickListener(v -> { if(timeline!=null) timeline.changeZoom(-0.2f); });
        zoomIn.setOnClickListener(v -> { if(timeline!=null) timeline.changeZoom(0.2f); });
        timelineHead.addView(zoomOut); LinearLayout.LayoutParams zip=new LinearLayout.LayoutParams(-2,dp(34)); zip.leftMargin=dp(4); timelineHead.addView(zoomIn,zip);
        root.addView(timelineHead);
        LinearLayout editTools = row();
        addTool(editTools,"SELECIONAR",()->toast("Modo de seleção ativo."));
        addTool(editTools,"DIVIDIR",this::splitSelected);
        addTool(editTools,"DUPLICAR",this::duplicateSelected);
        addTool(editTools,"EXCLUIR",this::deleteSelected);
        root.addView(editTools);
        timeline = new TimelineView();
        LinearLayout.LayoutParams tlp = new LinearLayout.LayoutParams(-1,dp(210)); tlp.topMargin=dp(8); root.addView(timeline,tlp);

        // Inspector, state is explicit
        addDivider(root, 14);
        LinearLayout inspector = card();
        LinearLayout ih = row();
        inspectorTitle = txt("INSPECTOR  ·  PROPRIEDADES", 10, 0xff25d8f5, true);
        ih.addView(inspectorTitle,new LinearLayout.LayoutParams(0,-2,1));
        ih.addView(txt("● ATIVO",9,0xff65e6b2,true)); inspector.addView(ih);
        inspectorBody = txt("Selecione um clipe na timeline para ver suas propriedades.",11,0xffaab7d2,false);
        LinearLayout.LayoutParams ibp = new LinearLayout.LayoutParams(-1,-2); ibp.topMargin=dp(12); inspector.addView(inspectorBody,ibp);
        root.addView(inspector);

        // Export options
        addDivider(root, 14);
        LinearLayout exportCard=card();
        exportCard.addView(txt("EXPORTAÇÃO 360°",11,Color.WHITE,true));
        TextView expHint=txt("Metadados espaciais Google Spatial Media V1",10,0xff8291b2,false);
        LinearLayout.LayoutParams ehp=new LinearLayout.LayoutParams(-1,-2); ehp.topMargin=dp(5); exportCard.addView(expHint,ehp);
        stereoSpinner=new Spinner(this);
        ArrayAdapter<String> adapter=new ArrayAdapter<>(this,android.R.layout.simple_spinner_dropdown_item,new String[]{"Monoscópico · 360°", "Estéreo · Top / Bottom", "Estéreo · Left / Right"});
        stereoSpinner.setAdapter(adapter); stereoSpinner.setBackground(bg(0xff151e34,dp(9),0xff344767));
        LinearLayout.LayoutParams sp=new LinearLayout.LayoutParams(-1,dp(44)); sp.topMargin=dp(10); exportCard.addView(stereoSpinner,sp);
        exportButton=smallButton("EXPORTAR MP4 360°",true); exportButton.setEnabled(false); exportButton.setAlpha(.45f);
        LinearLayout.LayoutParams ebp=new LinearLayout.LayoutParams(-1,dp(46)); ebp.topMargin=dp(10); exportCard.addView(exportButton,ebp); exportButton.setOnClickListener(v->chooseOutput());
        status=txt("AGUARDANDO MÍDIA",10,0xff25d8f5,true); LinearLayout.LayoutParams stp=new LinearLayout.LayoutParams(-1,-2); stp.topMargin=dp(10); exportCard.addView(status,stp);
        root.addView(exportCard);

        TextView disclaimer=txt("NOTA: esta versão organiza clipes na timeline e injeta metadados 360° no MP4 selecionado. A montagem multipista com renderização contínua ainda está em desenvolvimento. O vídeo de origem precisa ser equiretangular (geralmente 2:1).",9,0xff687999,false);
        LinearLayout.LayoutParams np=new LinearLayout.LayoutParams(-1,-2); np.topMargin=dp(14); root.addView(disclaimer,np);
        setContentView(scroll);
    }

    private LinearLayout row(){ LinearLayout l=new LinearLayout(this); l.setOrientation(LinearLayout.HORIZONTAL); l.setGravity(Gravity.CENTER_VERTICAL); return l; }
    private LinearLayout col(){ LinearLayout l=new LinearLayout(this); l.setOrientation(LinearLayout.VERTICAL); return l; }
    private LinearLayout card(){ LinearLayout l=col(); l.setPadding(dp(12),dp(12),dp(12),dp(12)); l.setBackground(bg(0xff101729,dp(14),0xff253450)); LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,-2); p.topMargin=dp(10); return l; }
    private TextView txt(String s,int size,int color,boolean bold){ TextView t=new TextView(this); t.setText(s); t.setTextSize(size); t.setTextColor(color); if(bold)t.setTypeface(Typeface.DEFAULT,Typeface.BOLD); return t; }
    private GradientDrawable bg(int color,int radius,int stroke){ GradientDrawable d=new GradientDrawable(); d.setColor(color); d.setCornerRadius(radius); if(stroke!=0)d.setStroke(dp(1),stroke); return d; }
    private Button smallButton(String s,boolean primary){ Button b=new Button(this); b.setText(s); b.setTextSize(10); b.setTypeface(Typeface.DEFAULT,Typeface.BOLD); b.setAllCaps(false); b.setTextColor(primary?0xff071323:0xffdce7ff); b.setBackground(bg(primary?0xff26d8f3:0xff1b2946,dp(9),primary?0:0xff354c70)); b.setPadding(dp(8),0,dp(8),0); b.setMinHeight(dp(48)); return b; }
    private Button transportButton(String s){ Button b=smallButton(s,false); b.setMinWidth(dp(48)); b.setPadding(dp(5),0,dp(5),0); return b; }
    private int dp(float n){return (int)(n*getResources().getDisplayMetrics().density+.5f);}
    private void addDivider(LinearLayout root,int margin){ View v=new View(this); v.setBackgroundColor(0xff25324b); LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,dp(1)); p.topMargin=dp(margin); p.bottomMargin=dp(margin); root.addView(v,p); }
    private void addTool(LinearLayout row,String label,Runnable action){ Button b=smallButton(label,false); b.setOnClickListener(v->action.run()); LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,dp(36),1); p.rightMargin=dp(4); row.addView(b,p); }
    private void toast(String s){Toast.makeText(this,s,Toast.LENGTH_SHORT).show();}
    private void updatePanel(){ String[] names={"EDIÇÃO", "COR", "ÁUDIO", "CONTROLES 360°"}; inspectorTitle.setText("PAINEL  ·  "+names[panel]); String[] info={"Selecione um clipe na timeline. Use Dividir, Duplicar e Excluir para organizar os segmentos.","Controles de cor serão aplicados nesta área nas próximas versões.","Controles de volume, fades e waveform serão adicionados nesta área.","Prévia esférica: arraste para girar e use pinça para alterar o campo de visão."}; inspectorBody.setText(info[panel]); }

    private void chooseVideo(){ Intent i=new Intent(Intent.ACTION_OPEN_DOCUMENT); i.addCategory(Intent.CATEGORY_OPENABLE); i.setType("video/*"); i.putExtra(Intent.EXTRA_ALLOW_MULTIPLE,true); startActivityForResult(i,REQUEST_VIDEO); }
    private void chooseOutput(){ if(selected<0||selected>=clips.size()){toast("Importe e selecione um clipe primeiro.");return;} Intent i=new Intent(Intent.ACTION_CREATE_DOCUMENT); i.addCategory(Intent.CATEGORY_OPENABLE); i.setType("video/mp4"); i.putExtra(Intent.EXTRA_TITLE,"nebula360_export.mp4"); startActivityForResult(i,REQUEST_OUTPUT); }
    @Override protected void onActivityResult(int request,int result,Intent data){ super.onActivityResult(request,result,data); if(result!=RESULT_OK||data==null)return; if(request==REQUEST_VIDEO){ if(data.getClipData()!=null){ for(int x=0;x<data.getClipData().getItemCount();x++) importUri(data.getClipData().getItemAt(x).getUri()); } else if(data.getData()!=null) importUri(data.getData()); } else if(request==REQUEST_OUTPUT&&data.getData()!=null) exportVideo(data.getData()); }
    private void importUri(Uri uri){ try{getContentResolver().takePersistableUriPermission(uri,Intent.FLAG_GRANT_READ_URI_PERMISSION);}catch(Exception ignored){} status.setText("IMPORTANDO MÍDIA…"); worker.execute(()->copyAndRegister(uri)); }
    private void copyAndRegister(Uri uri){ try{ int index=clips.size(); File f=new File(getCacheDir(),"nebula_clip_"+System.nanoTime()+".mp4"); try(InputStream in=getContentResolver().openInputStream(uri); OutputStream out=new FileOutputStream(f)){if(in==null)throw new IOException("Não foi possível abrir o arquivo."); byte[] buf=new byte[1024*512];int n;while((n=in.read(buf))>0)out.write(buf,0,n);} MediaMetadataRetriever r=new MediaMetadataRetriever();r.setDataSource(this,uri);String title=r.extractMetadata(MediaMetadataRetriever.METADATA_KEY_TITLE);String dur=r.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION);String w=r.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH);String h=r.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT);r.release(); long ms=dur==null?0:Long.parseLong(dur); String name=(title==null||title.trim().isEmpty()?"Clip "+(index+1):title); int[] colors={0xff176e9c,0xff5b459e,0xff147d78,0xff9b5d32}; Clip c=new Clip(name,f.getAbsolutePath(),uri,ms,colors[index%colors.length]); main.post(()->{clips.add(c);selected=clips.size()-1;sourcePath=c.path;surfaceUri=c.uri;mediaCount.setText(clips.size()+" CLIPS");details.setText("Selecionado: "+c.name+"  ·  "+(ms/1000)+"s  ·  "+w+"×"+h);timeline.invalidate();updateInspector();preview.setVideoUri(uri);exportButton.setEnabled(true);exportButton.setAlpha(1f);status.setText("MÍDIA IMPORTADA  ·  "+c.name);startPreviewIfReady();}); }catch(Exception e){main.post(()->status.setText("ERRO AO IMPORTAR: "+e.getMessage()));} }
    private void updateInspector(){ if(selected>=0&&selected<clips.size()){Clip c=clips.get(selected);inspectorBody.setText("Clipe: "+c.name+"\nDuração: "+String.format(Locale.US,"%.2f s",c.durationMs/1000.0)+"\nOrigem: arquivo MP4\nFaixa: V1 · Vídeo 360°\n\nAções disponíveis: dividir, duplicar, excluir e selecionar para exportação.");}else inspectorBody.setText("Selecione um clipe na timeline para ver suas propriedades."); }
    private void selectClip(int index){if(index<0||index>=clips.size())return;selected=index;Clip c=clips.get(index);sourcePath=c.path;surfaceUri=c.uri;preview.setVideoUri(c.uri);details.setText("Selecionado: "+c.name+"  ·  "+(c.durationMs/1000)+"s");updateInspector();timeline.invalidate();startPreviewIfReady();}
    private void splitSelected(){if(selected<0){toast("Selecione um clipe.");return;}Clip c=clips.get(selected);if(c.durationMs<2000){toast("Clipe muito curto para dividir.");return;}Clip second=new Clip(c.name+" · B",c.path,c.uri,c.durationMs/2,0xff5b459e);c.durationMs=c.durationMs/2;clips.add(selected+1,second);selected++;timeline.invalidate();updateInspector();mediaCount.setText(clips.size()+" CLIPS");toast("Segmento dividido na metade (organização da timeline).");}
    private void duplicateSelected(){if(selected<0){toast("Selecione um clipe.");return;}Clip c=clips.get(selected);clips.add(selected+1,new Clip(c.name+" · cópia",c.path,c.uri,c.durationMs,0xff5b459e));selected++;timeline.invalidate();updateInspector();mediaCount.setText(clips.size()+" CLIPS");}
    private void deleteSelected(){if(selected<0){toast("Selecione um clipe.");return;}clips.remove(selected);selected=clips.isEmpty()?-1:Math.min(selected,clips.size()-1);timeline.invalidate();mediaCount.setText(clips.size()+" CLIPS");if(selected>=0)selectClip(selected);else{sourcePath=null;details.setText("Timeline vazia.");exportButton.setEnabled(false);updateInspector();}}

    private void startPreviewIfReady(){if(surfaceTexture==null||surfaceUri==null)return;releasePlayer();try{videoSurface=new android.view.Surface(surfaceTexture);player=new MediaPlayer();player.setDataSource(this,surfaceUri);player.setSurface(videoSurface);player.setLooping(true);player.setOnPreparedListener(mp->{try{surfaceTexture.setDefaultBufferSize(mp.getVideoWidth(),mp.getVideoHeight());mp.start();}catch(Exception e){status.setText("PRÉVIA INDISPONÍVEL: "+e.getMessage());}});player.setOnErrorListener((mp,w,e)->{status.setText("CODEC NÃO SUPORTADO NA PRÉVIA.");return true;});player.prepareAsync();}catch(Exception e){status.setText("ERRO NA PRÉVIA: "+e.getMessage());}}
    @Override public void onSurfaceReady(android.graphics.SurfaceTexture texture){surfaceTexture=texture;startPreviewIfReady();}
    private void togglePlayback(){if(player==null){toast("Importe um vídeo primeiro.");return;}try{if(player.isPlaying())player.pause();else player.start();}catch(Exception ignored){}}
    private void seekStart(){if(player!=null)player.seekTo(0);timecode.setText("00:00:00:00");}
    private void seekEnd(){if(player!=null){int d=player.getDuration();player.seekTo(Math.max(0,d-100));timecode.setText("FIM DO CLIPE");}}
    private void exportVideo(Uri destination){if(selected<0)return;Clip c=clips.get(selected);exportButton.setEnabled(false);status.setText("INJETANDO METADADOS ESPACIAIS…");int mode=stereoSpinner.getSelectedItemPosition();worker.execute(()->{File outFile=new File(getCacheDir(),"nebula360_export.mp4");try{if(outFile.exists())outFile.delete();String result=NativeInjector.inject(c.path,outFile.getAbsolutePath(),mode);if(!"OK".equals(result))throw new Exception(result==null?"Erro nativo":result);try(InputStream in=new FileInputStream(outFile);OutputStream out=getContentResolver().openOutputStream(destination,"w")){if(out==null)throw new Exception("Não foi possível criar destino.");byte[] b=new byte[1024*512];int n;while((n=in.read(b))>0)out.write(b,0,n);}main.post(()->status.setText("EXPORTADO: MP4 COM METADADOS 360° · CLIPE SELECIONADO"));}catch(Exception e){main.post(()->status.setText("FALHA NA EXPORTAÇÃO: "+e.getMessage()));}finally{main.post(()->exportButton.setEnabled(true));}});}
    private void releasePlayer(){try{if(player!=null){player.stop();player.release();}}catch(Exception ignored){}player=null;try{if(videoSurface!=null)videoSurface.release();}catch(Exception ignored){}videoSurface=null;}
    @Override protected void onPause(){super.onPause();if(preview!=null)preview.onPause();if(player!=null&&player.isPlaying())player.pause();}
    @Override protected void onResume(){super.onResume();if(preview!=null)preview.onResume();if(player!=null)try{player.start();}catch(Exception ignored){}}
    @Override protected void onDestroy(){releasePlayer();worker.shutdown();super.onDestroy();}

    private class TimelineView extends View {
        private final android.graphics.Paint p=new android.graphics.Paint(3); private float zoom=1f, scrollX=0; private float pxPerSecond=42f; private int trackHeight; private float downX;
        TimelineView(){super(MainActivity.this);setBackground(bg(0xff0a1020,dp(10),0xff293a59));trackHeight=dp(54);setLayerType(View.LAYER_TYPE_SOFTWARE,null);}
        void changeZoom(float z){zoom=Math.max(.5f,Math.min(3f,zoom+z));invalidate();}
        @Override protected void onDraw(android.graphics.Canvas c){super.onDraw(c);float w=getWidth(),h=getHeight();p.setColor(0xff111c31);c.drawRect(0,0,w,dp(28),p);p.setColor(0xff7e91b7);p.setTextSize(dp(9));for(int t=0;t<60;t+=5){float x=dp(54)+t*pxPerSecond*zoom-scrollX;if(x<dp(54)||x>w)continue;c.drawLine(x,dp(20),x,dp(28),p);c.drawText(String.format(Locale.US,"%02d:%02d",t/60,t%60),x+dp(2),dp(12),p);}String[] tracks={"V1  VIDEO", "A1  AUDIO", "T1  TEXT"};for(int r=0;r<3;r++){float y=dp(30)+r*trackHeight;p.setColor(0xff111a2c);c.drawRect(0,y,w,y+trackHeight-dp(2),p);p.setColor(0xff8da0c4);p.setTextSize(dp(9));c.drawText(tracks[r],dp(5),y+dp(19),p);c.drawLine(dp(52),y,w,y,p);}float x=dp(58);for(int i=0;i<clips.size();i++){Clip clip=clips.get(i);float cw=Math.max(dp(76),(clip.durationMs/1000f)*pxPerSecond*zoom);float y=dp(34);p.setColor(clip.color);c.drawRoundRect(x,y,x+cw,y+trackHeight-dp(10),dp(6),dp(6),p);p.setColor(0xffdff8ff);p.setTextSize(dp(10));c.save();c.clipRect(x+dp(5),y+dp(2),x+cw-dp(3),y+dp(24));c.drawText(clip.name,x+dp(7),y+dp(17),p);c.restore();p.setColor(0x557deaff);for(float thumb=x+dp(6);thumb<x+cw-dp(10);thumb+=dp(25))c.drawLine(thumb,y+dp(28),thumb,y+dp(42),p);if(i==selected){p.setStyle(android.graphics.Paint.Style.STROKE);p.setStrokeWidth(dp(2));p.setColor(0xff2be2ff);c.drawRoundRect(x,y,x+cw,y+trackHeight-dp(10),dp(6),dp(6),p);p.setStyle(android.graphics.Paint.Style.FILL);}x+=cw+dp(5);}p.setColor(0xffffd36a);p.setStrokeWidth(dp(2));float playX=dp(58)+Math.max(0,scrollX%Math.max(1,w-dp(60)));c.drawLine(playX,dp(20),playX,h,p);p.setColor(0xffffd36a);c.drawCircle(playX,dp(25),dp(4),p);if(clips.isEmpty()){p.setColor(0xff60708e);p.setTextSize(dp(11));c.drawText("Importe mídia para preencher a timeline",dp(62),dp(82),p);}}
        @Override public boolean onTouchEvent(android.view.MotionEvent e){if(e.getAction()==android.view.MotionEvent.ACTION_DOWN){downX=e.getX();return true;}if(e.getAction()==android.view.MotionEvent.ACTION_UP){float x=dp(58);for(int i=0;i<clips.size();i++){float cw=Math.max(dp(76),(clips.get(i).durationMs/1000f)*pxPerSecond*zoom);if(e.getX()>=x&&e.getX()<=x+cw){selectClip(i);return true;}x+=cw+dp(5);}if(Math.abs(e.getX()-downX)>dp(12)){scrollX=Math.max(0,scrollX+downX-e.getX());invalidate();}return true;}if(e.getAction()==android.view.MotionEvent.ACTION_MOVE){scrollX=Math.max(0,scrollX+downX-e.getX());downX=e.getX();invalidate();return true;}return true;}
    }
}
