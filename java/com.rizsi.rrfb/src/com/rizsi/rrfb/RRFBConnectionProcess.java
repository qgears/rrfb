package com.rizsi.rrfb;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.ProcessBuilder.Redirect;
import java.util.ArrayList;
import java.util.List;

public class RRFBConnectionProcess extends RRFBConnection {
	Process p;
	public RRFBConnectionProcess(String command, List<String> commandArg, boolean commandSplit) throws IOException {
		List<String> pieces=new ArrayList<>();
		if(commandSplit)
		{
			String[] pieces0=command.split(" ");
			for(String s: pieces0)
			{
				pieces.add(s);
			}
		}else
		{
			pieces.add(command);
		}
		pieces.addAll(commandArg);
		System.err.println("Connect command: "+pieces);
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
