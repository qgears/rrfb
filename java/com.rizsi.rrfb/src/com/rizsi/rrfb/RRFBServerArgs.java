package com.rizsi.rrfb;

import joptsimple.annot.JODelegate;
import joptsimple.annot.JOHelp;

public class RRFBServerArgs {
	@JOHelp("Bind the web server to this host.")
	public String host="127.0.0.1";
	@JOHelp("Bind the web server to this port.")
	public int port=9012;
	@JODelegate(prefix="connect")
	public RRFBConnectArgs connect=new RRFBConnectArgs();
}
