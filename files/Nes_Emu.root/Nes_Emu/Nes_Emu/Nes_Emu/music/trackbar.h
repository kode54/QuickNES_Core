#ifndef _MUSICMUSIC_TRACKBAR_H_
#define _MUSICMUSIC_TRACKBAR_H_

/**
 * \file trackbar.h
 * trackbar custom control
 * Copyright (C) 2005
 * \author musicmusic
 */

#include "window_helper.h"

class uxtheme_handle;
#ifndef _UXTHEME_H_
typedef HANDLE HTHEME;
#endif

namespace musicmusic
{
	/**
	* Class for host of trackbar control to (optionally) implement to recieve callbacks.
	* \see track_bar::set_callback
	*/
	class track_bar_host
	{
	public:
		/**
		* Called when thumb position changes.
		*
		* \param [in]	pos			Contains new position of track bar thumb
		* \param [in]	b_tracking	Specifies that the user is dragging the thumb if true
		*
		* \see track_bar::set_range
		*/
		virtual void on_position_change(unsigned pos, bool b_tracking) {};

		/**
		* Called to retrieve tooltip text when tracking
		*
		* \param [in]	pos			Contains position of track bar thumb
		* \param [out]	p_out		Recieves tooltip text, utf-8 encoded
		*
		* \see track_bar::set_range, track_bar::set_show_tooltips
		*/
		virtual void get_tooltip_text(unsigned pos, CString & p_out) {};
	};

	/**
	* Track bar base class. 
	*
	* Implemented by trackbar clients, used by host.
	*
	* \see container_window::create, container_window::destroy
	*/
	class track_bar : public container_window
	{
	public:
		/**
		* Message handler for track bar.
		*
		* \note						Do not override!
		*
		* \param [in]	wnd			Specifies window recieving the message
		* \param [in]	msg			Specifies message code
		* \param [in]	wp			Specifies message-specific WPARAM code
		* \param [in]	lp			Specifies message-specific LPARAM code
		*
		* \return					Message-specific value
		*/
		virtual LRESULT WINAPI on_message(HWND wnd,UINT msg,WPARAM wp,LPARAM lp);

		/**
		* Sets position of trackbar thumb
		*
		* \param [in]	pos			Specifies new position of track bar thumb
		*
		* \note	No range checking is done!
		*
		* \see track_bar::set_range
		*/
		void set_position(unsigned pos);

		/**
		* Retrieves position of trackbar thumb
		*
		* \return					Position of track bar thumb
		*
		* \see track_bar::set_range
		*/
		unsigned get_position() const;

		/**
		* Sets range of trackbar control
		*
		* \param [in]	min			New minimum position of thumb
		* \param [in]	max			New maximum position of thumb
		*
		*/
		void set_range(unsigned min, unsigned max);

		/**
		* Retrieves range of trackbar control
		*
		* \param [out]	min			Minimum position of thumb
		* \param [out]	max			Maximum position of thumb
		*
		*/
		void get_range(unsigned & min, unsigned & max) const;

		/**
		* Sets enabled state of track bar
		*
		* \param [in]	b_enabled	New enabled state
		*
		* \note The window is enabled by default
		*/
		void set_enabled(bool b_enabled);

		/**
		* Retrieves enabled state of track bar
		*
		* \return					Enabled state
		*/
		bool get_enabled() const;

		/**
		* Retrieves hot state of track bar thumb
		*
		* \return					Hot state. The thumb is hot when the mouse is over it
		*/
		bool get_hot() const;

		/**
		* Retrieves thumb rect for given position and range
		*
		* \param [in]	pos			Position of thumb
		* \param [in]	range_min		Minimum position of thumb
		* \param [in]	range_max		Maximum position of thumb
		* \param [out]	rc			Receives thumb rect
		*
		* \note Track bar implementations must implement this function
		*/
		virtual void get_thumb_rect(unsigned pos, unsigned range_min, unsigned range_max, RECT * rc) const = 0;

		/**
		* Retrieves thumb rect for current position and range (helper)
		*
		* \param [out]	rc			Receives thumb area in client co-ordinates
		*/
		void get_thumb_rect(RECT * rc) const;

		/**
		* Retrieves channel rect
		*
		* \param [out]	rc			Receives channel area in client co-ordinates
		*
		* \note Track bar implementations must implement this function
		*/
		virtual void get_channel_rect(RECT * rc) const = 0;

		/**
		* Draws track bar thumb
		*
		* \param [in]	dc			Handle to device context for the drawing operation
		* \param [in]	rc			Rectangle specifying the thumb area in client co-ordinates
		*
		* \note Track bar implementations must implement this function
		*/
		virtual void draw_thumb (HDC dc, const RECT * rc) = 0;

		/**
		* Draws track bar channel
		*
		* \param [in]	dc			Handle to device context for the drawing operation
		* \param [in]	rc			Rectangle specifying the channel area in client co-ordinates
		*
		* \note Track bar implementations must implement this function
		*/
		virtual void draw_channel (HDC dc, const RECT * rc) = 0;

		/**
		* Draws track bar background
		*
		* \param [in]	dc			Handle to device context for the drawing operation
		* \param [in]	rc			Rectangle specifying the background area in client co-ordinates
		*							This is equal to the client area.
		*
		* \note Track bar implementations must implement this function
		*/
		virtual void draw_background (HDC dc, const RECT * rc) const = 0;

		/**
		* Sets host callback interface
		*
		* \param [in]	p_host		pointer to host interface
		*
		* \note The pointer must be valid until the track bar is destroyed, or until a
		* subsequent call to this function
		*/
		void set_callback(track_bar_host * p_host);

		/**
		* Sets whether tooltips are shown
		*
		* \param [in]	b_show		specifies whether to show tooltips while tracking
		*
		* \note Tooltips are disabled by default
		*/
		void set_show_tooltips(bool b_show);

		/**
		* Default constructor for track_bar class
		*/
		track_bar()
			: m_theme(0), m_position(0), m_range_min(0), m_range_max(0), m_thumb_hot(0), m_host(0), 
			m_display_position(0), m_dragging(false), m_show_tooltips(false), m_wnd_prev(0),
			m_wnd_tooltip(0), m_uxtheme(0)
		{};
		~track_bar();
	protected:
		/**
		* Retreives a reference to the uxtheme_handle pointer
		*
		* \return					Reference to the uxtheme_handle pointer.
		*							May be empty in case uxtheme was not loaded.
		*/
		const uxtheme_handle * get_uxtheme_ptr() const;
		uxtheme_handle * get_uxtheme_ptr();
		/**
		* Retreives a handle to the theme context to be used for calling uxtheme APIs
		*
		* \return					Handle to the theme.
		* \par
		*							May be NULL in case theming is not active.
		*							In this case, use non-themed rendering.
		* \par
		*							The returned handle must not be released!
		*/
		HTHEME get_theme_handle() const;
	private:
		/**
		* Used internally by the track bar.\n
		* Sets position of the thumb and re-renders regions invalided as a result.
		*
		* \param [in]	pos			Position of the thumb, within the specified range.
		*
		* \see set_range
		*/
		void set_position_internal(unsigned pos);

		/**
		* Used internally by the track bar.\n
		* Used to update the hot status of the thumb when the mouse cursor moves.
		*
		* \param [in]	pt			New position of the mouse pointer, in client co-ordinates.
		*/
		void update_hot_status(POINT pt);

		/**
		* Used internally by the track bar.\n
		* Creates a tracking tooltip.
		*
		* \param [in]	text		Text to display in the tooltip.
		* \param [in]	pt			Position of the mouse pointer, in screen co-ordinates.
		*/
		BOOL create_tooltip(const TCHAR * text, POINT pt);

		/**
		* Used internally by the track bar.\n
		* Destroys the tracking tooltip.
		*/
		void destroy_tooltip();

		/**
		* Used internally by the track bar.\n
		* Updates position and text of tooltip.
		*
		* \param [in]	pt			Position of the mouse pointer, in screen co-ordinates.
		* \param [in]	text		Text to display in the tooltip.
		*/
		BOOL update_tooltip(POINT pt, const TCHAR * text);

		/**
		* Handle to the theme used for theme rendering.
		*/
		HTHEME m_theme;

		/**
		* Pointer to the uxtheme API.
		*/
		uxtheme_handle * m_uxtheme;

		/**
		* Current position of the thumb.
		*/
		unsigned m_position;
		/**
		* Minimum position of the thumb.
		*/
		unsigned m_range_min;
		/**
		* Maximum position of the thumb.
		*/
		unsigned m_range_max;

		/**
		* Hot state of the thumb.
		*/
		bool m_thumb_hot;

		/**
		* Tracking state of the thumb.
		*/
		bool m_dragging;

		/**
		* Current display position of the thumb. Used when tracking.
		*/
		unsigned m_display_position;

		/**
		* Show tooltips state.
		*/
		bool m_show_tooltips;

		/**
		* Window focus was obtained from.
		*/
		HWND m_wnd_prev;

		/**
		* Handle to tooltip window.
		*/
		HWND m_wnd_tooltip;

		virtual class_data & get_class_data()const ;

		/**
		* Pointer to host interface.
		*/
		track_bar_host * m_host;
	};

	/**
	* Track bar implementation, providing standard Windows rendering.
	*
	* \see track_bar, container_window::create, container_window::destroy
	*/
	class track_bar_impl : public track_bar
	{
		virtual void get_thumb_rect(unsigned pos, unsigned range_min, unsigned range_max, RECT * rc) const;
		virtual void get_channel_rect(RECT * rc) const;
		virtual void draw_thumb (HDC dc, const RECT * rc);
		virtual void draw_channel (HDC dc, const RECT * rc);
		virtual void draw_background (HDC dc, const RECT * rc) const;
	};
}

#endif
