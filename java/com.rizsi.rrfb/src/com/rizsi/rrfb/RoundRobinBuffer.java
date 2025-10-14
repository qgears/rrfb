package com.rizsi.rrfb;

/** A buffer to be circularly reused for buffering data.
 * Allows multithreaded access for reads and writes. Keeps book of written and read bytes
 * and blocks allocation for write until there is space in it.
 *  */
public class RoundRobinBuffer {
	private byte[] data;
	private int send;
	private volatile long allocated;
	private volatile long freed;
	private int currentLen;
	public RoundRobinBuffer(int size) {
		data=new byte[size];
	}

	/** signal that a buffer was sent and its content can be reused as a receive buffer.
	 * 
	 * Can be called on any thread.
	 * @param n
	 */
	public void sent(int n) {
		synchronized (this) {
			freed+=n;
		}
	}

	/** Signal that n bytes of the last allocated buffer was used.
	 * Must be called on the writer thread.
	 *  */
	public void used(int n) {
		allocated+=n;
		send+=n;
		if(send>=data.length)
		{
			send-=data.length;
		}
	}

	/** Get the offset to write the buffer to. */
	public int getOff() {
		return send;
	}

	/** Get the usable length of the current buffer view. */
	public int getLen() {
		return currentLen;
	}
	
	/** Allocate a buffer for write. The offset and length has to be queried after this call.
	 * Must be used on the writer thread. Blocks if necessary in 1ms granularity.
	 * @return
	 * @throws InterruptedException
	 */
	public byte[] nextBuffer() throws InterruptedException {
		long available = allocated-freed+data.length;
		while(available<1)
		{
			Thread.sleep(1);
			available = allocated-freed+data.length;
		}
		currentLen = (int)Math.min(data.length-send, available);
		return data;
	}
}
