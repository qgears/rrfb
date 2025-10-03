package com.rizsi.rrfb;

import joptsimple.annot.JOHelp;

public class RRFBConnectArgs {
	@JOHelp("If set: command to start when a client connect to stream rrfb protocol. Must be path to rrfb executable.")
	public String command;
	@JOHelp("TCP host to connect to to stream rrfb protocol.")
	public String host;
	@JOHelp("TCP port to connect to to stream rrfb protocol.")
	public int port;
	public RRFBConnection connect() throws Exception {
		if(command!=null)
		{
			return new RRFBConnectionProcess(command);
		}else
		{
			return new RRFBConnectionTcp(host, port);
		}
	}
}
