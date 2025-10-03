package com.rizsi.rrfb;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.ProcessBuilder.Redirect;
import java.util.List;

import hu.qgears.commons.UtilString;

public class RRFBConnectionProcess extends RRFBConnection {
	Process p;
	public RRFBConnectionProcess(String command) throws IOException {
		List<String> pieces=UtilString.split(command, " ");
		p=new ProcessBuilder(pieces).redirectError(Redirect.INHERIT).start();
	}
	@Override
	protected InputStream getInputStream() {
		return p.getInputStream();
	}
	@Override
	protected OutputStream getOutputStream() {
		return p.getOutputStream();
	}
	@Override
	public void close() {
		if(p!=null)
		{
			p.destroyForcibly();
			p=null;
		}
	}
}
