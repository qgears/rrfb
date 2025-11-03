package com.rizsi.rrfb;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

import hu.qgears.commons.ConnectStreams;

public class RRFBConnectionReverseTcp extends RRFBConnection {
	Socket socket;
	FileInputStream fis;
	FileOutputStream fos;
	File in, out;
	public RRFBConnectionReverseTcp(String host, int port) throws Exception {
		socket=new Socket(host, port);
		long t=System.currentTimeMillis();
		in=new File("/tmp/rrfb_in_"+t);
		out=new File("/tmp/rrfb_out_"+t);
        Files.deleteIfExists(in.toPath());
        new ProcessBuilder("mkfifo", in.toPath().toString())
                .inheritIO()
                .start()
                .waitFor();
        new ProcessBuilder("chmod", "666", in.toPath().toString())
        	.inheritIO().start().waitFor();
        Files.deleteIfExists(out.toPath());
        new ProcessBuilder("mkfifo", out.toPath().toString())
                .inheritIO()
                .start()
                .waitFor();
        ConnectStreams.startStreamThread(socket.getInputStream(), System.out);
        socket.getOutputStream().write((out.getAbsolutePath()+"\n").getBytes(StandardCharsets.UTF_8));
        socket.getOutputStream().write((in.getAbsolutePath()+"\n").getBytes(StandardCharsets.UTF_8));
        socket.getOutputStream().flush();
	}
	@Override
	protected InputStream getInputStream() throws Exception {
		if(fis==null)
		{
			fis=new FileInputStream(in);
		}
		return fis;
	}
	@Override
	protected OutputStream getOutputStream() throws Exception {
		if(fos==null)
		{
	        fos=new FileOutputStream(out);
		}
		return fos;
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
		if(fis!=null)
		{
			try {
				fis.close();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
			fis=null;
		}
		if(fos!=null)
		{
			try {
				fos.close();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
			fos=null;
		}
		if(in!=null)
		{
	        try {
				Files.deleteIfExists(in.toPath());
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
	        in=null;
		}
		if(out!=null)
		{
	        try {
				Files.deleteIfExists(out.toPath());
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
	        out=null;
		}
	}
}
