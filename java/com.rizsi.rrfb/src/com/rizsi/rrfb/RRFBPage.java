package com.rizsi.rrfb;

import hu.qgears.quickjs.utils.SimpleHttpPage;

public class RRFBPage extends SimpleHttpPage {
	@SuppressWarnings("unused")
	private String name;
	public RRFBPage(String name) {
		this.name=name;
	}
	@Override
	public void writeHeaders() {
		super.writeHeaders();
		write("<script> module={};</script>\n<script src=\"../res/decode.js\"></script>\n<script src=\"../res/rrfb.js\"></script>\n<style>html, body {margin: 0; height: 100%; overflow: hidden}\n.floating-menu {\n    background: yellowgreen;\n    z-index: 100;\n    position: fixed;\n    top: 0px;\n    left: 0px;\n  }\n</style>\n");
	}
	@Override
	public void writeBody() {
		write("<div id=\"floatmenu\" class=\"floating-menu\">\n<h3>RRFB Floating menu</h3>\nF1 makes the menu visible\n<button id=\"floatmenu_hide\">Hide</button>\nFrames look forward: <span id=\"nframe\"></span> <button id=\"nframep\">+</button><button id=\"nframem\">-</button>\nTarget period time ms: <span id=\"targetms\"></span> <button id=\"targetmsp\">+</button><button id=\"targetmsm\">-</button>\n\n</div>\n<canvas id=\"myCanvas\" width=\"1920\" height=\"1080\"></canvas>\n<a href='../res/rrfb.js'>rrfb.js</a>");
	}
}
