import java.io.*;
import java.nio.file.*;
import java.util.*;

public class RCMRelocator {

	public static String[] readLines(String path) {
		try {
			byte[] b = Files.readAllBytes(Paths.get(path));
			String asStr = new String(b);
			
			//replace \r\n with \n for simplicity
			asStr = asStr.replaceAll("\r\n", "\n");
			asStr = asStr.replaceAll("\r", "\n");
			return asStr.split("\n");
			
		} catch(IOException ex) {
			return null;
		}
	}
	
	public static String getSymType(String[] syms, String sym) {
		for(String s : syms) {
			try {
				String sansAddress = s.substring(9); //8-char address+space
				String type = sansAddress.substring(0, 7);
				String sectionSizeName = sansAddress.substring(8);
				sectionSizeName = sectionSizeName.replaceAll("\\s+", " ").trim();
				String[] fields = sectionSizeName.split(" ");
				String section = fields[0];
				String name = fields[2];
				if(!section.equals("*ABS*") && !section.equals("*UND*") && name.equals(sym))
					return type;
			} catch(Exception ex) {
				//invalid line, just skip it
			}
		}
		return null;
	}
	
	public static int getSymAddress(String[] syms, String sym) {
		for(String s : syms) {
			try {
				String sectionSizeName = s.substring(17);
				sectionSizeName = sectionSizeName.replaceAll("\\s+", " ").trim();
				String[] fields = sectionSizeName.split(" ");
				String section = fields[0];
				String name = fields[2];
				String strAddr = s.substring(0, 8);
				int intAddr = Integer.parseInt(strAddr, 16);
				if(!section.equals("*ABS*") && !section.equals("*UND*") && name.equals(sym))
					return intAddr;
			} catch(Exception ex) {
				//invalid line, just skip it
			}
		}
		return 0;
	}
	
	public static boolean getSymExists(String[] syms, String sym) {
		return getSymType(syms, sym) != null;
	}
	
	public static int read32(byte[] b, int offset) {
		int b0 = ((int) b[offset + 0]) & 0xFF;
		int b1 = ((int) b[offset + 1]) & 0xFF;
		int b2 = ((int) b[offset + 2]) & 0xFF;
		int b3 = ((int) b[offset + 3]) & 0xFF;
		return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
	}
	
	public static void write32(byte[] b, int offset, int n) {
		b[offset + 0] = (byte) ((n >>>  0) & 0xFF);
		b[offset + 1] = (byte) ((n >>>  8) & 0xFF);
		b[offset + 2] = (byte) ((n >>> 16) & 0xFF);
		b[offset + 3] = (byte) ((n >>> 24) & 0xFF);
	}

	public static void main(String[] args) {
		//read arguments. First RCM name, then relocation output from objdump, then symbol output
		//from objdump
		
		if(args.length < 3) {
			System.err.println("[ERROR] Usage: java RCMRelocator <RCM File> <relocations> <symbols>");
			System.exit(1);
		}
		
		String rcmPath = args[0];
		String relocPath = args[1];
		String symPath = args[2];
		
		String[] relocs = readLines(relocPath);
		String[] syms = readLines(symPath);
		
		if(relocs == null || syms == null) {
			System.err.println("[ERROR] File not found.");
			System.exit(1);
		}
		
		List<Relocation> relocations = new ArrayList<>();
		
		//next up, process relocations in order
		for(String s : relocs) {
			try {
				String entry = s.replaceAll("\\s+", " ").trim();
				String[] info = entry.split(" ");
				
				String strOffset = info[0];
				String strInfo = info[1];
				String strType = info[2];
				String strValue = info[3];
				String strName = info[4];
				int intOffset = Integer.parseInt(strOffset, 16);
				int intInfo = Integer.parseInt(strInfo, 16);
				int intValue = Integer.parseInt(strValue, 16);
				
				relocations.add(new Relocation(intOffset, strType, intValue, getSymExists(syms, strName), strName));
			} catch(Exception ex) {
				//invalid line, discard
			}
		}
		
		//print relocations
		for(Relocation r : relocations) {
			System.out.println(r);
		}
		
		//generate relocation section
		try {
			byte[] rcm = Files.readAllBytes(Paths.get(rcmPath));
			ByteArrayOutputStream baos = new ByteArrayOutputStream();	//relocation table
			ByteArrayOutputStream thunks = new ByteArrayOutputStream();	//thunks (for ARM->THUMB B)
			int nRelocations = 0;
			try {
				//before relocation output, count the number of times each destination address shows up.
				Map<Integer, Integer> refCounts = new HashMap<>();
				for(Relocation r : relocations) {
					if(!r.local) {
						int addr = r.value;
						if(refCounts.get(addr) == null) refCounts.put(addr, 1);
						else refCounts.put(addr, refCounts.get(addr) + 1);
					}
				}
			
				//print out the number of times an address shows up
				for(Map.Entry<Integer, Integer> entry : refCounts.entrySet()) {
					if(entry.getValue() >= 3) System.out.printf("%08X\t%d\n", entry.getKey(), entry.getValue());
					//TODO: make it generate thunks for these >=3 instance relocations
				}
				
				//first pass: normal relocations. Only output actual relocations.
				boolean errUnresolved = false;
				for(Relocation r : relocations) {
					if(r.type.equals("R_ARM_V4BX")) continue; //ignore these
					
					//check: is destination THUMB, and is the instruction a B?
					//or: is it a Bxx that can be resolved without a relocation?
					boolean isThumb = (r.value & 1) == 1;
					boolean isB = r.type.equals("R_ARM_JUMP24");
					boolean isRelative = r.type.equals("R_ARM_JUMP24") || r.type.equals("R_ARM_CALL");
					boolean isLocal = r.local;
					boolean isAbs32 = r.type.equals("R_ARM_ABS32") && !isLocal;
					if(!(isThumb && isB) && !(isRelative && isLocal) && !isAbs32) {
						if(!(!isLocal && r.value == 0)) { //check external relocations for NULL
						
							//let's get smart, try to arrange this so that we may emit a zero-value
							int type = Relocation.relocTypeFromString(r.type, r.local);
							int orig;
							switch (type) {
								case Relocation.R_ARM_BASE_ABS:
								case Relocation.R_ARM_ABS32:
									//we can add to the data in the file.
									orig = read32(rcm, r.offset);
									orig += r.value;
									write32(rcm, r.offset, orig);
									r.value -= r.value;
									break;
								case Relocation.R_ARM_JUMP24:
								case Relocation.R_ARM_CALL:
									if (isThumb) break; //unavoidable. Value must exist
									orig = read32(rcm, r.offset);
									int instr = orig;
									
									//partial relocation; adjust value to 0, don't adjust by base
									int offset = (orig & 0x00FFFFFF) << 2;
									if ((offset & 0x02000000) != 0) offset -= 0x04000000;
									offset = ((offset + r.value) >> 2) & 0x00FFFFFF;
									orig = (instr & 0xFF000000) | offset;
									
									write32(rcm, r.offset, orig);
									r.value -= r.value;
									break;
							}
							
							
							baos.write(r.getBytes()); //write relocation as normal
							nRelocations++;
						} else {
							//external relocation points to NULL
							//probably means a symbol was not resolved, throw an error
							System.err.println("[ERROR] Unresolved external symbol " + r.name);
							errUnresolved = true;
						}
					}
				}
				if(errUnresolved) {
					System.exit(1); //only exit after showing all symbol errors
				}
				
				//second pass: resolve resolvable relocations
				int thunkOffset = rcm.length + baos.size();
				for(Relocation r : relocations) {
					if(r.type.equals("R_ARM_V4BX")) continue; //ignore these
					
					//check: is destination THUMB, and is the instruction a B? If so, create a thunk
					boolean isThumb = (r.value & 1) == 1;
					boolean isB = r.type.equals("R_ARM_JUMP24");
					boolean isRelative = r.type.equals("R_ARM_JUMP24") || r.type.equals("R_ARM_CALL");
					boolean isLocal = r.local;
					boolean isAbs32 = r.type.equals("R_ARM_ABS32") && !isLocal;
					if(isThumb && isB) {
						System.out.println("[DEBUG] --------------WRITING THUMB THUNK AT " + String.format("%08X", r.offset));
						
						//create thunk:  LDR PC, =(addr)
						int currentOffset = thunks.size();
						int dest = r.value;
						byte[] thunk = {
							(byte) 0x04, (byte) 0xF0, (byte) 0x1F, (byte) 0xE5,
							(byte) (dest & 0xFF),
							(byte) ((dest >>> 8) & 0xFF),
							(byte) ((dest >>> 16) & 0xFF),
							(byte) ((dest >>> 24) & 0xFF)
						};
						thunks.write(thunk);
						
						//overwrite original instruction
						int rel = (currentOffset + thunkOffset - (r.offset + 8)) >>> 2;
						byte[] newBranch = {
							(byte) (rel & 0xFF),
							(byte) ((rel >>> 8) & 0xFF),
							(byte) ((rel >>> 16) & 0xFF),
							(byte) 0xEA
						};
						rcm[r.offset + 0] = newBranch[0];
						rcm[r.offset + 1] = newBranch[1];
						rcm[r.offset + 2] = newBranch[2];
						rcm[r.offset + 3] = newBranch[3];
					} else if(isRelative && isLocal) {
						System.out.println("[DEBUG] --------------RESOLVING LOCAL BRANCH AT " + String.format("%08X", r.offset));
						
						int instrOffset = r.offset;
						int instr = (rcm[r.offset] & 0xFF) | ((rcm[r.offset + 1] & 0xFF) << 8)
							| ((rcm[r.offset + 2] & 0xFF) << 16)
							| ((rcm[r.offset + 3] & 0xFF) << 24);
						
						int offset = (instr & 0x00FFFFFF) << 2;
						if((offset & 0x02000000) != 0) offset -= 0x04000000;
						offset = ((offset + r.value - r.offset) >>> 2) & 0x00FFFFFF;
						instr = (instr & 0xFF000000) | offset;
						
						byte[] newBranch = {
							(byte) (instr & 0xFF),
							(byte) ((instr >>> 8) & 0xFF),
							(byte) ((instr >>> 16) & 0xFF),
							(byte) ((instr >>> 24) & 0xFF)
						};
						rcm[r.offset + 0] = newBranch[0];
						rcm[r.offset + 1] = newBranch[1];
						rcm[r.offset + 2] = newBranch[2];
						rcm[r.offset + 3] = newBranch[3];
					} else if(isAbs32) { //pointless to output these
						int orig = (rcm[r.offset] & 0xFF) | ((rcm[r.offset + 1] & 0xFF) << 8)
							| ((rcm[r.offset + 2] & 0xFF) << 16)
							| ((rcm[r.offset + 3] & 0xFF) << 24);
						orig += r.value;
						
						byte[] newValue = {
							(byte) (orig & 0xFF),
							(byte) ((orig >>> 8) & 0xFF),
							(byte) ((orig >>> 16) & 0xFF),
							(byte) ((orig >>> 24) & 0xFF)
						};
						rcm[r.offset + 0] = newValue[0];
						rcm[r.offset + 1] = newValue[1];
						rcm[r.offset + 2] = newValue[2];
						rcm[r.offset + 3] = newValue[3];
					}
				}
			} catch(IOException ex) {
				throw new IllegalStateException("What?");
			}
			byte[] relocBytes = baos.toByteArray();
			byte[] thunkBytes = thunks.toByteArray();
			
			//write relocation data
			int ofsReloc = rcm.length;
			int szReloc = nRelocations;
			rcm[4] = (byte) (ofsReloc & 0xFF);
			rcm[5] = (byte) ((ofsReloc >>> 8) & 0xFF);
			rcm[6] = (byte) (szReloc & 0xFF);
			rcm[7] = (byte) ((szReloc >>> 8) & 0xFF);
			
			File f = new File(rcmPath);
			OutputStream out = new FileOutputStream(f);
			out.write(rcm);
			out.write(relocBytes);
			out.write(thunkBytes);
			out.close();
			
		} catch(IOException ex) {
			ex.printStackTrace();
			System.exit(1);
		}
		
	}
	
}

class Relocation {

	public static final int ZERO_VALUE     = 0x80;
	public static final int R_ARM_CALL     = 28;
	public static final int R_ARM_ABS32    = 2;
	public static final int R_ARM_JUMP24   = 29;
	public static final int R_ARM_BASE_ABS = 31;
	public static final int R_ARM_V4BX     = 40;

	public static int relocTypeFromString(String type, boolean local) {
		//if local, make the type relative
		if(local) {
			switch(type) {
				case "R_ARM_ABS32":
					type = "R_ARM_BASE_ABS"; break;
				case "R_ARM_CALL":
				case "R_ARM_JUMP24":
					break;
				default:
					throw new IllegalStateException("Unknown relative relocation type " + type);
			}
		}
		
		switch(type) {
			case "R_ARM_CALL":
				return R_ARM_CALL;
			case "R_ARM_ABS32":
				return R_ARM_ABS32;
			case "R_ARM_JUMP24":
				return R_ARM_JUMP24;
			case "R_ARM_V4BX":
				return R_ARM_V4BX;
			case "R_ARM_BASE_ABS":
				return R_ARM_BASE_ABS;
			default:
				throw new IllegalStateException("Unknown relocation type " + type);
		}
	}
	
	public int offset;
	public String type;
	public int value;
	public boolean local;
	public String name;
	
	public Relocation(int offset, String type, int value, boolean local, String name) {
		this.offset = offset;
		this.type = type;
		this.value = value;
		this.local = local;
		this.name = name;
	}
	
	
	@Override
	public String toString() {
		return String.format("Offset %08X\tValue %08X\t%c\tType %s", this.offset, this.value, this.local ? ' ' : 'E', this.type);
	}
	
	//get byte representation of relocation
	public byte[] getBytes() {
		/*
			u32 offset : 24;
			u32 type : 8;
			u32 value;
		*/
		int w0 = (relocTypeFromString(this.type, this.local) << 24) | this.offset;
		int w1 = this.value;
		byte[] b;
		
		//new zero-value flag: can we optimize?
		if (this.value == 0) {
			w0 |= (Relocation.ZERO_VALUE << 24);
			b = new byte[] {
				(byte) (w0 & 0xFF),
				(byte) ((w0 >>> 8) & 0xFF),
				(byte) ((w0 >>> 16) & 0xFF),
				(byte) ((w0 >>> 24) & 0xFF)
			};
		} else {
			b = new byte[] {
				(byte) (w0 & 0xFF),
				(byte) ((w0 >>> 8) & 0xFF),
				(byte) ((w0 >>> 16) & 0xFF),
				(byte) ((w0 >>> 24) & 0xFF),
				(byte) (w1 & 0xFF),
				(byte) ((w1 >>> 8) & 0xFF),
				(byte) ((w1 >>> 16) & 0xFF),
				(byte) ((w1 >>> 24) & 0xFF)
			};
		}
		
		return b;
	}
	
}
