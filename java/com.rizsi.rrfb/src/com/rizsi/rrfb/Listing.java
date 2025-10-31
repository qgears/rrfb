package com.rizsi.rrfb;

import hu.qgears.quickjs.utils.SimpleHttpPage;

public class Listing extends SimpleHttpPage {
	RRFBServerArgs args;
	public Listing(RRFBServerArgs args) {
		this.args=args;
	}

	@Override
	protected void writeBody() {
		write("<h1>RRFB server</h2>\n");
		for(String key: args.allConnections.keySet())
		{
			write("<h2><a href=\"");
			writeHtml(key);
			write("/\">Desktop ");
			writeHtml(key);
			write("</a></h2>\n");
		}
		// TODO Auto-generated method stub
		super.writeBody();
	}
}
