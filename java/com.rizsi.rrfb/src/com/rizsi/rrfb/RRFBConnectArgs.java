package com.rizsi.rrfb;

import java.util.ArrayList;
import java.util.List;

import joptsimple.annot.JOHelp;

public class RRFBConnectArgs {
	@JOHelp("Name of the desktop shown to the user in the selection page.")
	public String name="unnamed";
	@JOHelp("If set: command to start when a client connect to stream rrfb protocol. Must be path to rrfb executable.")
	public String command;
	@JOHelp("Additional arguments of command that starts rrfb server process.")
	public List<String> commandArg=new ArrayList<String>();
	@JOHelp("TCP host to connect to to stream rrfb protocol.")
	public String host;
	@JOHelp("TCP port to connect to to stream rrfb protocol.")
	public int port;
	@JOHelp("Split the command by spaces to create command+Argument list (instead of using commandArg)")
	public boolean commandSplit = true;
	public RRFBConnection connect() throws Exception {
		if(command!=null)
		{
			return new RRFBConnectionProcess(command, commandArg, commandSplit);
		}else
		{
			return new RRFBConnectionTcp(host, port);
		}
	}
}
