package com.rizsi.rrfb;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;

public class RRFBConnectionTcp extends RRFBConnection {
	Socket socket;
	public RRFBConnectionTcp(String host, int port) throws IOException {
		socket=new Socket(host, port);
	}
	@Override
	protected InputStream getInputStream() {
		try {
			return socket.getInputStream();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			return null;
		}
	}
	@Override
	protected OutputStream getOutputStream() {
		try {
			return socket.getOutputStream();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			return null;
		}
	}
	@Override
	public void close() {
		if(socket!=null)
		{
			try {
				socket.close();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
			socket=null;
		}
	}
}
