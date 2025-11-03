package com.rizsi.rrfb;

import java.io.InputStream;
import java.io.OutputStream;

public abstract class RRFBConnection implements AutoCloseable {

	protected abstract InputStream getInputStream() throws Exception;

	protected abstract OutputStream getOutputStream() throws Exception;
	@Override
	abstract public void close();
}
