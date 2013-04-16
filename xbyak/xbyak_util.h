#ifndef XBYAK_XBYAK_UTIL_H_
#define XBYAK_XBYAK_UTIL_H_

/**
	utility class and functions for Xbyak
	Xbyak::util::Clock ; rdtsc timer
	Xbyak::util::Cpu ; detect CPU
	@note this header is UNDER CONSTRUCTION!
*/
#include "xbyak/xbyak.h"

#ifdef _WIN32
	#if (_MSC_VER < 1400) && defined(XBYAK32)
		static inline __declspec(naked) void __cpuid(int[4], int)
		{
			__asm {
				push	ebx
				push	esi
				mov		eax, dword ptr [esp + 4 * 2 + 8] // eaxIn
				cpuid
				mov		esi, dword ptr [esp + 4 * 2 + 4] // data
				mov		dword ptr [esi], eax
				mov		dword ptr [esi + 4], ebx
				mov		dword ptr [esi + 8], ecx
				mov		dword ptr [esi + 12], edx
				pop		esi
				pop		ebx
				ret
			}
		}
	#else
		#include <intrin.h> // for __cpuid
	#endif
#else
	#ifndef __GNUC_PREREQ
    	#define __GNUC_PREREQ(major, minor) ((((__GNUC__) << 16) + (__GNUC_MINOR__)) >= (((major) << 16) + (minor)))
	#endif
	#if __GNUC_PREREQ(4, 3) && !defined(__APPLE__)
		#include <cpuid.h>
	#else
		#if defined(__APPLE__) && defined(XBYAK32) // avoid err : can't find a register in class `BREG' while reloading `asm'
			#define __cpuid(eaxIn, a, b, c, d) __asm__ __volatile__("pushl %%ebx\ncpuid\nmovl %%ebp, %%esi\npopl %%ebx" : "=a"(a), "=S"(b), "=c"(c), "=d"(d) : "0"(eaxIn))
		#else
			#define __cpuid(eaxIn, a, b, c, d) __asm__ __volatile__("cpuid\n" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "0"(eaxIn))
		#endif
	#endif
#endif

#ifdef _MSC_VER
extern "C" unsigned __int64 __xgetbv(int);
#endif

namespace Xbyak { namespace util {

/**
	CPU detection class
*/
class Cpu {
	unsigned int type_;
	unsigned int get32bitAsBE(const char *x) const
	{
		return x[0] | (x[1] << 8) | (x[2] << 16) | (x[3] << 24);
	}
	unsigned int mask(int n) const
	{
		return (1U << n) - 1;
	}
	void setFamily()
	{
		unsigned int data[4];
		getCpuid(1, data);
		stepping = data[0] & mask(4);
		model = (data[0] >> 4) & mask(4);
		family = (data[0] >> 8) & mask(4);
		// type = (data[0] >> 12) & mask(2);
		extModel = (data[0] >> 16) & mask(4);
		extFamily = (data[0] >> 20) & mask(8);
		if (family == 0x0f) {
			displayFamily = family + extFamily;
		} else {
			displayFamily = family;
		}
		if (family == 6 || family == 0x0f) {
			displayModel = (extModel << 4) + model;
		} else {
			displayModel = model;
		}
	}
public:
	int model;
	int family;
	int stepping;
	int extModel;
	int extFamily;
	int displayFamily; // family + extFamily
	int displayModel; // model + extModel
	static inline void getCpuid(unsigned int eaxIn, unsigned int data[4])
	{
#ifdef _WIN32
		__cpuid(reinterpret_cast<int*>(data), eaxIn);
#else
		__cpuid(eaxIn, data[0], data[1], data[2], data[3]);
#endif
	}
	static inline uint64 getXfeature()
	{
#ifdef _MSC_VER
		return __xgetbv(0);
#else
		unsigned int eax, edx;
		// xgetvb is not support on gcc 4.2
//		__asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
		__asm__ volatile(".byte 0x0f, 0x01, 0xd0" : "=a"(eax), "=d"(edx) : "c"(0));
		return ((uint64)edx << 32) | eax;
#endif
	}
	enum Type {
		NONE = 0,
		tMMX = 1 << 0,
		tMMX2 = 1 << 1,
		tCMOV = 1 << 2,
		tSSE = 1 << 3,
		tSSE2 = 1 << 4,
		tSSE3 = 1 << 5,
		tSSSE3 = 1 << 6,
		tSSE41 = 1 << 7,
		tSSE42 = 1 << 8,
		tPOPCNT = 1 << 9,
		tAESNI = 1 << 10,
		tSSE5 = 1 << 11,
		tOSXSAVE = 1 << 12,
		tPCLMULQDQ = 1 << 13,
		tAVX = 1 << 14,
		tFMA = 1 << 15,

		t3DN = 1 << 16,
		tE3DN = 1 << 17,
		tSSE4a = 1 << 18,
		tRDTSCP = 1 << 19,

		tINTEL = 1 << 24,
		tAMD = 1 << 25
	};
	Cpu()
		: type_(NONE)
	{
		unsigned int data[4];
		getCpuid(0, data);
		static const char intel[] = "ntel";
		static const char amd[] = "cAMD";
		if (data[2] == get32bitAsBE(amd)) {
			type_ |= tAMD;
			getCpuid(0x80000001, data);
			if (data[3] & (1U << 31)) type_ |= t3DN;
			if (data[3] & (1U << 15)) type_ |= tCMOV;
			if (data[3] & (1U << 30)) type_ |= tE3DN;
			if (data[3] & (1U << 22)) type_ |= tMMX2;
			if (data[3] & (1U << 27)) type_ |= tRDTSCP;
		}
		if (data[2] == get32bitAsBE(intel)) {
			type_ |= tINTEL;
			getCpuid(0x80000001, data);
			if (data[3] & (1U << 27)) type_ |= tRDTSCP;
		}
		getCpuid(1, data);
		if (data[2] & (1U << 0)) type_ |= tSSE3;
		if (data[2] & (1U << 9)) type_ |= tSSSE3;
		if (data[2] & (1U << 19)) type_ |= tSSE41;
		if (data[2] & (1U << 20)) type_ |= tSSE42;
		if (data[2] & (1U << 23)) type_ |= tPOPCNT;
		if (data[2] & (1U << 25)) type_ |= tAESNI;
		if (data[2] & (1U << 1)) type_ |= tPCLMULQDQ;
		if (data[2] & (1U << 27)) type_ |= tOSXSAVE;

		if (type_ & tOSXSAVE) {
			// check XFEATURE_ENABLED_MASK[2:1] = '11b'
			uint64 bv = getXfeature();
			if ((bv & 6) == 6) {
				if (data[2] & (1U << 28)) type_ |= tAVX;
				if (data[2] & (1U << 12)) type_ |= tFMA;
			}
		}

		if (data[3] & (1U << 15)) type_ |= tCMOV;
		if (data[3] & (1U << 23)) type_ |= tMMX;
		if (data[3] & (1U << 25)) type_ |= tMMX2 | tSSE;
		if (data[3] & (1U << 26)) type_ |= tSSE2;
		setFamily();
	}
	void putFamily()
	{
		printf("family=%d, model=%X, stepping=%d, extFamily=%d, extModel=%X\n",
			family, model, stepping, extFamily, extModel);
		printf("display:family=%X, model=%X\n", displayFamily, displayModel);
	}
	bool has(Type type) const
	{
		return (type & type_) != 0;
	}
};

class Clock {
public:
	static inline uint64 getRdtsc()
	{
#ifdef _MSC_VER
		return __rdtsc();
#else
		unsigned int eax, edx;
		__asm__ volatile("rdtsc" : "=a"(eax), "=d"(edx));
		return ((uint64)edx << 32) | eax;
#endif
	}
	Clock()
		: clock_(0)
		, count_(0)
	{
	}
	void begin()
	{
		clock_ -= getRdtsc();
	}
	void end()
	{
		clock_ += getRdtsc();
		count_++;
	}
	int getCount() const { return count_; }
	uint64 getClock() const { return clock_; }
	void clear() { count_ = 0; clock_ = 0; }
private:
	uint64 clock_;
	int count_;
};

#ifdef XBYAK64
const int UseRCX = 1 << 6;
const int UseRDX = 1 << 7;

class StackFrame {
#ifdef XBYAK64_WIN
	static  const int noSaveNum = 6;
#else
	static const int noSaveNum = 8;
#endif
	Xbyak::CodeGenerator *code_;
	int pNum_;
	int tNum_;
	bool useRcx_;
	bool useRdx_;
	int saveNum_;
	int P_;
	bool makeEpilog_;
	Xbyak::Reg64 pTbl_[4];
	Xbyak::Reg64 tTbl_[10];
public:
	const Xbyak::Reg64& p(int pos) const
	{
		if (pos < 0 || pos >= pNum_) throw ERR_BAD_PARAMETER;
		return pTbl_[pos];
	}
	const Xbyak::Reg64& t(int pos) const
	{
		if (pos < 0 || pos >= tNum_) throw ERR_BAD_PARAMETER;
		return tTbl_[pos];
	}
	/*
		make stack frame
		@param sf [in] this
		@param pNum [in] num of global parameter
		@param tNum [in] num of temporary register
		@param numQword [in] local stack size
		@param useReg [in] reserve rcx, rdx if necessary
		you can use
		rax
		gp0, ..., gp(pNum - 1)
		gt0, ..., gt(tNum-1)
		rsp[0..8 * numQrod - 1]
	*/
	StackFrame(Xbyak::CodeGenerator *code, int pNum, int tNum = 0, int numQword = 0)
		: code_(code)
		, pNum_(pNum)
		, tNum_(tNum & ~(UseRCX | UseRDX))
		, useRcx_((tNum & UseRCX) != 0)
		, useRdx_((tNum & UseRDX) != 0)
		, saveNum_(0)
		, P_(0)
		, makeEpilog_(true)
	{
		using namespace Xbyak;
		if (pNum < 0 || pNum > 4) throw ERR_BAD_PNUM;
		const int allRegNum = pNum + tNum_ + (useRcx_ ? 1 : 0) + (useRdx_ ? 1 : 0);
		if (allRegNum < pNum || allRegNum > 14) throw ERR_BAD_TNUM;
		const Reg64& rsp = code->rsp;
		const AddressFrame& ptr = code->ptr;
		saveNum_ = (std::max)(0, allRegNum - noSaveNum);
		const int *tbl = getOrderTbl() + noSaveNum;
		P_ = saveNum_ + numQword;
		if (P_ > 0 && (P_ & 1) == 0) P_++; // ensure (rsp % 16) == 0
		P_ *= 8;
		if (P_ > 0) code->sub(rsp, P_);
#ifdef XBYAK64_WIN
		for (int i = 0; i < (std::min)(saveNum_, 4); i++) {
			code->mov(ptr [rsp + P_ + (i + 1) * 8], Reg64(tbl[i]));
		}
		for (int i = 4; i < saveNum_; i++) {
			code->mov(ptr [rsp + P_ - 8 * (saveNum_ - i)], Reg64(tbl[i]));
		}
#else
		for (int i = 0; i < saveNum_; i++) {
			code->mov(ptr [rsp + P_ - 8 * (saveNum_ - i)], Reg64(tbl[i]));
		}
#endif
		int pos = 0;
		for (int i = 0; i < pNum; i++) {
			pTbl_[i] = Xbyak::Reg64(getRegIdx(pos));
		}
		for (int i = 0; i < tNum_; i++) {
			tTbl_[i] = Xbyak::Reg64(getRegIdx(pos));
		}
	}
	void disableEpilog() { makeEpilog_ = false; }
	void close(bool callRet = true)
	{
		using namespace Xbyak;
		const Reg64& rsp = code_->rsp;
		const AddressFrame& ptr = code_->ptr;
		const int *tbl = getOrderTbl() + noSaveNum;
#ifdef XBYAK64_WIN
		for (int i = 0; i < (std::min)(saveNum_, 4); i++) {
			code_->mov(Reg64(tbl[i]), ptr [rsp + P_ + (i + 1) * 8]);
		}
		for (int i = 4; i < saveNum_; i++) {
			code_->mov(Reg64(tbl[i]), ptr [rsp + P_ - 8 * (saveNum_ - i)]);
		}
#else
		for (int i = 0; i < saveNum_; i++) {
			code_->mov(Reg64(tbl[i]), ptr [rsp + P_ - 8 * (saveNum_ - i)]);
		}
#endif
		if (P_ > 0) code_->add(rsp, P_);

		if (callRet) code_->ret();
	}
	~StackFrame()
	{
		if (makeEpilog_) close();
	}
private:
	const int *getOrderTbl() const
	{
		using namespace Xbyak;
		static const int tbl[] = {
#ifdef XBYAK64_WIN
			Operand::RCX, Operand::RDX, Operand::R8, Operand::R9, Operand::R10, Operand::R11, Operand::RDI, Operand::RSI,
#else
			Operand::RDI, Operand::RSI, Operand::RDX, Operand::RCX, Operand::R8, Operand::R9, Operand::R10, Operand::R11,
#endif
			Operand::RBX, Operand::RBP, Operand::R12, Operand::R13, Operand::R14, Operand::R15
		};
		return &tbl[0];
	}
	int getRegIdx(int& pos) const
	{
		assert(pos < 14);
		using namespace Xbyak;
		const int *tbl = getOrderTbl();
		int r = tbl[pos++];
		if (useRcx_) {
			if (r == Operand::RCX) {
				code_->mov(code_->r10, code_->rcx);
				return Operand::R10;
			}
			if (r == Operand::R10) { r = tbl[pos++]; }
		}
		if (useRdx_) {
			if (r == Operand::RDX) {
				code_->mov(code_->r11, code_->rdx);
				return Operand::R11;
			}
			if (r == Operand::R11) { return tbl[pos++]; }
		}
		return r;
	}
};
#endif

} } // end of util
#endif
