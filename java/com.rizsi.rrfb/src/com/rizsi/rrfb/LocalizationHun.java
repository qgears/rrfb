package com.rizsi.rrfb;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.Map;

import hu.qgears.quickjs.qpage.ILocalizationInterface;

public class LocalizationHun implements ILocalizationInterface {
	Map<String,String> trans=new HashMap<>();
	DateFormat dfShort=createDateFormatShortLocalized();
	DateFormat df=createDateFormatLocalized();
	DateFormat dfLong=createDateFormatLongLocalized();
	/**
	 * Create a localized dateformat object.
	 * TODO actually it is not localized - yet.
	 * @return
	 */
	public static DateFormat createDateFormatShortLocalized() {
		return new SimpleDateFormat("yyyy.MM.dd.");
	}
	/**
	 * Create a localized dateformat object.
	 * TODO actually it is not localized - yet.
	 * @return
	 */
	public static DateFormat createDateFormatLocalized() {
		return new SimpleDateFormat("yyyy.MM.dd. HH:mm");
	}
	/**
	 * Create a localized dateformat object.
	 * TODO actually it is not localized - yet.
	 * @return
	 */
	public static DateFormat createDateFormatLongLocalized() {
		return new SimpleDateFormat("yyyy.MM.dd. HH:mm:ss EEE");
	}

	public LocalizationHun() {
		try {
			trans=new HashMap<>(); //Ini.loadIni(UtilFile.loadAsString(getClass().getResource("translations_HU.ini")));
		} catch (Exception e) {
			e.printStackTrace();
		}
	}
	@Override
	public String getString(String id, Object[] args) {
		String s=trans.get(id);
		return stringFormat(s, args);
	}
	
	/**
	 * Safe version of String.format(...
	 * Does not throw exception in case of invalid pattern. In case of exception the parameters are all concatenated.
	 * @param string
	 * @param args
	 * @return
	 */
	public static String stringFormat(String string, Object... args) {
		try
		{
			return String.format(string, args);
		}catch(Exception e)
		{
			// Silent fail here
		}
		StringBuilder ret=new StringBuilder();
		if(string!=null)
		{
			ret.append(string);
		}else
		{
			ret.append("null");
		}
		for(Object o: args)
		{
			ret.append(""+o);
		}
		return ret.toString();
	}
	@Override
	public String formatDateShort(Date date) {
		return dfShort.format(date);
	}
	@Override
	public String formatDate(Date date) {
		synchronized (this) {
			return df.format(date);
		}
	}
	public String formatDateLong(Date date) {
		synchronized (this) {
			return dfLong.format(date);
		}
	};
}
