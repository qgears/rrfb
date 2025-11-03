package com.rizsi.rrfb;

import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;

import org.eclipse.jetty.websocket.api.Session;
import org.eclipse.jetty.websocket.api.WebSocketAdapter;
import org.eclipse.jetty.websocket.api.WriteCallback;

public class RRFBWs extends WebSocketAdapter {
	RRFBServerArgs args;
	RRFBConnectArgs rrfbConnectArgs;
	public RRFBWs(RRFBServerArgs args, RRFBConnectArgs rrfbConnectArgs) {
		this.args=args;
		this.rrfbConnectArgs=rrfbConnectArgs;
	}
	Session sess;
	OutputStream out;
	RRFBConnection conn;
	public void onWebSocketConnect(org.eclipse.jetty.websocket.api.Session sess) {
		this.sess=sess;
		try {
			try {
				conn=rrfbConnectArgs.connect();
				new Thread("RRFBoutStream")
				{
				public void run() {
					try {
						InputStream is=conn.getInputStream();
						System.out.println("RRFBWs input opened.");
						RoundRobinBuffer rrb=new RoundRobinBuffer(1024*1024*16);
						while(true)
						{
							byte[] buffer=rrb.nextBuffer();
							int off=rrb.getOff();
							int len=rrb.getLen();
							int n=is.read(buffer, off, len);
							if(n<1)
							{
								throw new EOFException();
							}
							rrb.used(n);
							processInsertTimestamps(buffer, off, n);
							sess.getRemote().sendBytes(ByteBuffer.wrap(buffer, off, n), new WriteCallback(){
								public void writeSuccess() {
									rrb.sent(n);
								};
								public void writeFailed(Throwable x) {
									x.printStackTrace();
									sess.close();
								};
							});
						}
					}catch(EOFException e)
					{
						System.err.println("RRFB output stream EOF reached");
					}catch(IOException e)
					{
						System.err.println("RRFB output stream closed");
					} catch (Exception e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					} finally
					{
						try
						{
							conn.close();
						}catch(NullPointerException e)
						{
							// normal - do not log
						}catch(Exception e)
						{
							e.printStackTrace();
						}
					}
				};
				}
				.start();
				out=conn.getOutputStream();
				System.out.println("RRFBWs output opened.");
				System.out.println("Connected!");
			} catch (Exception e) {
				throw new RuntimeException(e);
			}
		} catch (Exception e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	private int processState=0;
	private int afterSkipState;
	private int remainingPayload=0;
	private int processHeaderNbyte=0;
	private int overwritten=0;
	private int afterTimestampState;
	private int command2Size;
	private byte[] header=new byte[8];
	private byte[] timestamp=new byte[4];
	private ByteBuffer headerWrapped=ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN);
	private ByteBuffer timestampWrapped=ByteBuffer.wrap(timestamp).order(ByteOrder.LITTLE_ENDIAN);
	protected void processInsertTimestamps(byte[] buffer, int off, int n) {
		while(n>0)
		{
			switch(processState)
			{
			case 0:
				// Gather header
				while (processHeaderNbyte<8 && n>0)
				{
					header[processHeaderNbyte]=buffer[off];
					n--;
					off++;
					processHeaderNbyte++;
				}
				if(processHeaderNbyte==8)
				{
					int command=headerWrapped.getInt(0);
					int size=headerWrapped.getInt(4);
					// System.out.println("Command: "+command+" size: "+size);
					processState=1;
					remainingPayload=size;
					processHeaderNbyte=0;
					switch(command)
					{
					case 6:
						processState=1;
						remainingPayload-=4;
						afterSkipState=2;
						overwritten=0;
						timestampWrapped.putInt(0, (int)(System.currentTimeMillis()%4294967296l));
						afterTimestampState=0;
						break;
					case 2:
						command2Size=size;
						processState=1;
						remainingPayload=4;
						afterSkipState=2;
						overwritten=0;
						timestampWrapped.putInt(0, (int)(System.currentTimeMillis()%4294967296l));
						afterTimestampState=3;
						break;
					default:
						afterSkipState=0;
						break;
					}
				}
				break;
			case 1:
			{
				int skip=Math.min(remainingPayload, n);
				off+=skip;
				n-=skip;
				remainingPayload-=skip;
				if(remainingPayload==0)
				{
					processState=afterSkipState;
				}
				break;
			}
			case 2:	// Insert timestamp at last 4 bytes of payload
				while (overwritten<4 && n>0)
				{
					buffer[off]=timestamp[overwritten];
					n--;
					off++;
					overwritten++;
				}
				if(overwritten==4)
				{
					processState=afterTimestampState;
				}
				break;
			case 3: // Insert timestamp done on command2 first timestamp
			{
				processState=1;
				remainingPayload=command2Size-8-4;
				afterSkipState=4;
				break;
			}
			case 4:
			{
				// All data received in command 2: second timestamp
				processState=2;
				overwritten=0;
				timestampWrapped.putInt(0, (int)(System.currentTimeMillis()%4294967296l));
				afterTimestampState=0;
				break;
			}
			default:
				throw new RuntimeException();
			}
		}
		// TODO Auto-generated method stub
	}
	@Override
	public void onWebSocketClose(int statusCode, String reason) {
		if(conn!=null)
		{
			conn.close();
			conn=null;
		}
		System.out.println("Disconnected");
	}
	@Override
	public void onWebSocketText(String message) {
		if(out!=null)
		{
		try {
			String pattern= "\"msgServerTimestamp\":0";
			int idx=message.indexOf(pattern);
			String replaced;
			if(idx>0)
			{
				replaced=message.substring(0,idx)+"\"msgServerTimestamp\":"+ (System.currentTimeMillis()%4294967296l) +message.substring(idx+pattern.length());
			}else
			{
				replaced=message;
			}
			// System.out.println("WS messge: "+message+"\n"+replaced);
			out.write(replaced.getBytes(StandardCharsets.UTF_8));
			out.write("\n".getBytes(StandardCharsets.UTF_8));
			out.flush();
		} catch (Exception e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			out=null;
		}
		}
	}
}
