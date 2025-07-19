#pragma once

#include "BlockDirective.hpp"

class Location : public BlockDirective {
	private:
		char* uri;
		bool exactMatch;

		Location(const Location& other);
		Location& operator=(const Location& other);

	public:
		Location(void);
		~Location(void);
		DIRTYPE getType(void) const;
		void setUri(char* uri);
		void setExactMatch(bool value);
		bool validate(void);
		char* getUri() const{return uri;}
		bool isExactMatch() const{return exactMatch;}
};

