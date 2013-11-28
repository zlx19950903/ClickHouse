#pragma once

#include <Poco/Net/HTTPServerResponse.h>

#include <DB/Core/Exception.h>
#include <DB/Core/ErrorCodes.h>

#include <DB/IO/WriteBuffer.h>
#include <DB/IO/BufferWithOwnMemory.h>


namespace DB
{


/** Отличается от WriteBufferFromOStream тем, что инициализируется не std::ostream, а Poco::Net::HTTPServerResponse.
  * При первом сбросе данных, получает из него std::ostream (с помощью метода send).
  * Это нужно в HTTP серверах, чтобы после передачи в какой-нибудь метод WriteBuffer-а,
  *  но до вывода первых данных клиенту, можно было изменить какие-нибудь HTTP заголовки (например, код ответа).
  * (После вызова Poco::Net::HTTPServerResponse::send() изменить заголовки уже нельзя.)
  * То есть, суть в том, чтобы вызывать метод Poco::Net::HTTPServerResponse::send() не сразу.
  */
class WriteBufferFromHTTPServerResponse : public BufferWithOwnMemory<WriteBuffer>
{
private:
	Poco::Net::HTTPServerResponse & response;
	std::ostream * ostr;

	void nextImpl()
	{
		if (!ostr)
			ostr = &response.send();
		
		if (!offset())
			return;
		
		ostr->write(working_buffer.begin(), offset());
		ostr->flush();

		if (!ostr->good())
			throw Exception("Cannot write to ostream", ErrorCodes::CANNOT_WRITE_TO_OSTREAM);
	}

public:
	WriteBufferFromHTTPServerResponse(Poco::Net::HTTPServerResponse & response_, size_t size = DBMS_DEFAULT_BUFFER_SIZE)
		: BufferWithOwnMemory<WriteBuffer>(size), response(response_), ostr(NULL) {}

	~WriteBufferFromHTTPServerResponse()
	{
		if (!offset())
			return;

		try
		{
			if (!ostr)
				ostr = &response.send();
			
			next();
		}
		catch (...)
		{
			tryLogCurrentException(__PRETTY_FUNCTION__);
		}
	}
};

}
