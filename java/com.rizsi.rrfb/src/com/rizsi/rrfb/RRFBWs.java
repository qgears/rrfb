package com.rizsi.rrfb;

import java.io.EOFException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

import org.eclipse.jetty.websocket.api.Session;
import org.eclipse.jetty.websocket.api.WebSocketAdapter;

public class RRFBWs extends WebSocketAdapter {
	RRFBServerArgs args;
	public RRFBWs(RRFBServerArgs args) {
		this.args=args;
	}
	Session sess;
	OutputStream out;
	RRFBConnection conn;
	public void onWebSocketConnect(org.eclipse.jetty.websocket.api.Session sess) {
		this.sess=sess;
		try {
			try {
				conn=args.connect.connect();
				InputStream is=conn.getInputStream();
				out=conn.getOutputStream();
				new Thread("RRFBoutStream")
				{
				public void run() {
					try {
						byte[]buffer=new byte[32768];
						while(true)
						{
							int n=is.read(buffer);
							if(n<1)
							{
								throw new EOFException();
							}
							sess.getRemote().sendBytes(ByteBuffer.wrap(buffer, 0, n));
						}
					} catch (Exception e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					} finally
					{
						try
						{
							conn.close();
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
