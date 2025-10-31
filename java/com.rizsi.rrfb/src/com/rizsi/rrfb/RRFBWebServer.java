package com.rizsi.rrfb;

import java.net.InetSocketAddress;
import java.security.SecureRandom;
import java.util.List;

import org.eclipse.jetty.server.Request;
import org.eclipse.jetty.server.Server;
import org.eclipse.jetty.server.session.DefaultSessionIdManager;

import com.rizsi.rrfb.res.Res;

import hu.qgears.quickjs.qpage.HtmlTemplate;
import hu.qgears.quickjs.qpage.jetty.websocket.WebSocketCreationContext;
import hu.qgears.quickjs.qpage.jetty.websocket.WebSocketCreator;
import hu.qgears.quickjs.qpage.jetty.websocket.WebSocketSimple;
import hu.qgears.quickjs.utils.DispatchHandler;
import hu.qgears.quickjs.utils.ResourceClassPathHandler;
import hu.qgears.quickjs.utils.gdpr.GdprSessionHandler;
import hu.qgears.quickjs.utils.gdpr.GdprSessionsSetup;
import hu.qgears.slf4j.consoleimpl.Log4Init;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import joptsimple.annot.AnnotatedClass;

public class RRFBWebServer {
	public static void main(String[] args) throws Exception {
		Log4Init.init();
		List<String> remainingArgs;
		RRFBServerArgs argso=new RRFBServerArgs();
		{
			AnnotatedClass ac=new AnnotatedClass();
			ac.parseAnnotations(argso);
			ac.parseArgs(args);
			ac.print();
			argso.addConnection(argso.connect);
			remainingArgs=ac.nonOptionArguments();
		}
		while(remainingArgs.size()>0)
		{
			RRFBAdditionalArgs add=new RRFBAdditionalArgs();
			AnnotatedClass ac=new AnnotatedClass();
			ac.parseAnnotations(add);
			ac.parseArgs(remainingArgs.toArray(new String[] {}));
			ac.print();
			argso.addConnection(add.connect);
			remainingArgs=ac.nonOptionArguments();
		}
		new RRFBWebServer().run(argso);
	}
	public int run(RRFBServerArgs args) throws Exception
	{
		InetSocketAddress sa = new InetSocketAddress(args.host, args.port);
		
		Server server = new Server(sa);
		
		HtmlTemplate.setLocalizationSupplier(()->new LocalizationHun());
		
		GdprSessionHandler sessionHandler=GdprSessionsSetup.setup(server, "rrfb");
		// Specify the Session ID Manager
		DefaultSessionIdManager idmanager = new DefaultSessionIdManager(server, new SecureRandom());
		server.setSessionIdManager(idmanager);
		
		DispatchHandler dh=new DispatchHandler() {
			protected void preHandle(String target, Request baseRequest, HttpServletRequest request, HttpServletResponse response) {
			};
		};
		for(String name: args.allConnections.keySet())
		{
			WebSocketSimple wss=new WebSocketSimple(new WebSocketCreator() {
				@Override
				public Object createWebSocket(WebSocketCreationContext c) {
					return new RRFBWs(args, args.allConnections.get(name));
				}
			});
			dh.addHandler("/"+name, "/", new RRFBPage(name).createHandler());
			dh.addHandler("/"+name, "/rrfb", wss.createHandler());
		}
		dh.addHandler("/", new Listing(args).createHandler());
		dh.addHandler("/res", new ResourceClassPathHandler(Res.class));
		dh.addHandler("/res", new ResourceClassPathHandler(Res.class));
		sessionHandler.setHandler(dh);
		server.start();
		server.join();
		
		return 0;
	}
}
