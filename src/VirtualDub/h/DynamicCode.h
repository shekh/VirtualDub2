struct DynamicCodeBlock {
	void	*pCode;
	long	cbCode;
	short	nEntryPoints;
	short	nRelocs;

	long	entrypts[1];
};

class DynamicCode {
protected:
	void **pDynamicBlock;

public:
	DynamicCode(const DynamicCodeBlock *, long *params);
	~DynamicCode();

	void *getEntryPoint(int ep) { return pDynamicBlock[ep]; }
};
