package com.rizsi.rrfb;

import java.io.InputStream;
import java.io.OutputStream;

public abstract class RRFBConnection implements AutoCloseable {

	protected abstract InputStream getInputStream();

	protected abstract OutputStream getOutputStream();
	@Override
	abstract public void close();
}
