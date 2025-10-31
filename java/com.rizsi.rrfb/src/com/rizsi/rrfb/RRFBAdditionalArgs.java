package com.rizsi.rrfb;

import joptsimple.annot.JODelegate;

public class RRFBAdditionalArgs {
	@JODelegate(prefix="connect")
	public RRFBConnectArgs connect=new RRFBConnectArgs();
}
