package com.rizsi.rrfb;

import java.util.Map;
import java.util.TreeMap;

import joptsimple.annot.JODelegate;
import joptsimple.annot.JOHelp;
import joptsimple.annot.JOSkip;

public class RRFBServerArgs {
	@JOHelp("Bind the web server to this host.")
	public String host="127.0.0.1";
	@JOHelp("Bind the web server to this port.")
	public int port=9012;
	@JODelegate(prefix="connect")
	public RRFBConnectArgs connect=new RRFBConnectArgs();
	@JOSkip
	public Map<String, RRFBConnectArgs> allConnections=new TreeMap<>();
	public void addConnection(RRFBConnectArgs connect) {
		allConnections.put(connect.name, connect);
	}
}
