package com.rizsi.rrfb;

import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
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
				InputStream is=conn.getInputStream();
				out=conn.getOutputStream();
				new Thread("RRFBoutStream")
				{
				public void run() {
					try {
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
				System.out.println("Connected!");
			} catch (Exception e) {
				throw new RuntimeException(e);
			}
		} catch (Exception e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
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
//		System.out.println("WS messge: "+message);
		try {
			out.write(message.getBytes(StandardCharsets.UTF_8));
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
