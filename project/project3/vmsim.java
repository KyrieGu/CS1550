

import java.util.*;
import java.io.*;
import java.lang.*;




public class vmsim {




  public static void main (String[] args) throws Exception {

    int nFrame = -1;   //number of frames
    String mode = null;  //type of algorithms
    int refresh = -1;  //refresh period
    String fileName = null;    //input trace file name

    File file;    //file need to open

    for(int i=0; i<args.length; i++){

      //grab the number of frames
      if(args[i].equals("-n")){
        nFrame = Integer.parseInt(args[i+1]);
        continue;
      }

      //set up the algorithm type
      if(args[i].equals("-a")){
        mode = args[i+1].toLowerCase();
        continue;
      }

      //set up the refresh period
      if(args[i].equals("-r")){
        refresh = Integer.parseInt(args[i+1]);
        continue;
      }

    }

    //set up the file name
    fileName = args[args.length-1].toLowerCase();

    //READY TO CHOOSE ALGORITHM
    switch(mode) {

      //Use optimal page table
      case "opt":
      opt(nFrame,fileName);
      break;

      //Use first in first out algorithm
      case "fifo":
      fifo(nFrame,fileName);
      break;

      //Use aging algorithm
      case "aging":
      age(nFrame,fileName,refresh);
      break;

      default:
    }

  }


  //optimal replacement algorithm
  public static void opt(int nFrame,String fileName) throws Exception {

    int pageFaults = 0;
    int memoryAcc = 0;
    int writes = 0;

    //declare hash maps
    HashMap<Integer,PTE>  pageTable = new HashMap<Integer,PTE>();   //declare page table
    HashMap<Integer,LinkedList<Integer>> ref = new HashMap<Integer,LinkedList<Integer>>();   //declare prediction list

    //declare the physical address
    int[] frames = new int[nFrame];


    //create a BufferedReader to read trace file line by line
    BufferedReader reader = new BufferedReader(new FileReader(fileName));



    //Initialize the page table
    int size = (int)Math.pow(2,20);    //we need 20 bits to store the whole 32 bit address
    for(int i=0; i < size; i++){
      PTE pte = new PTE(i);    //create a new default page entry
      pageTable.put(i,pte);
      ref.put(i,new LinkedList<Integer>());
    }

    //intilize the physical frames
    for(int i=0; i< nFrame; i++){
      frames[i] = -1;
    }


    //intilize the predicted reference trace -- first time run the program
    int lines = 0;  //number of lines
    String line = reader.readLine();  //current line

    while(line != null){
      String[] array = line.split(" ");   //split input line
      for(int i =0; i<array.length;i++){
        array[i].toLowerCase();
      }
      int address = Integer.parseInt(array[1].substring(2,7),16);   //convert the address to decimal
      int offset = Integer.parseInt(array[1].substring(7,10),16);   //convert offset to decimal
      //store the current line with the address
      ref.get(address).add(lines);
      lines++;
      line = reader.readLine();
    }

    //close the buffer
    reader.close();


    //Now we run the program second time -- apply opt
    reader = new BufferedReader(new FileReader(fileName));
    line = reader.readLine();
    lines = 0;

    while(line != null){
      String[] array = line.split(" ");   //split input line
      for(int i =0; i<array.length;i++){
        array[i].toLowerCase();
      }
      int address = Integer.parseInt(array[1].substring(2,7),16);   //convert the address to decimal
      int offset = Integer.parseInt(array[1].substring(7,10),16);   //convert offset to decimal
      //System.out.println("Address is " + address);
      //set up a pte
      PTE pte = pageTable.get(address);   //get the pte from the page table
      pte.referenced = true;


      //check whether it is a "store" instruction
      if(array[0].equals("s")){
        pte.dirty = true;
      }

      //check if it is already in the memory
      if(pte.valid == true){
        //System.out.println(array[1] +  " hit");
        //remove the finished reference
        ref.get(address).removeFirst();
      }

      //if not in the memory
      else{
        //increase the number of page faults
        pageFaults++;
        //first try to find the suitable frame to store
        boolean found = false;
        for(int i=0; i<nFrame; i++){

          if(frames[i] == -1){
            //This frame is available, we map a virtual page to it
            pte.frame = i;
            //store the address to the frame
            frames[i] = address;

            found = true;
            //System.out.println(array[1] + " page fault - no eviction");
            break;
          }
        }

        //if no free frame, we need to evict the furthest one
        if(!found){
          //find the frame with the largest referenced line
          int position = 0;
          if(!ref.get(frames[position]).isEmpty()){
            int max = ref.get(frames[position]).peek();
            for(int i=0; i<nFrame; i++){
              if(ref.get(frames[i]).isEmpty()){
                position = i;
                break;
              }
              else if (max < ref.get(frames[i]).peek()){
                position = i;
                max = ref.get(frames[i]).peek();
              }
            }
          }

          //evict this frame
          if(pageTable.get(frames[position]).dirty == false){
            //System.out.println(array[1] + " page fault - evict clean ");
            //writes++;   //we need to store the data to disk
          }

          else{
            //System.out.println(array[1] + " page fault - evict dirty ");
            writes++;   //we need to store the data to disk
            pageTable.get(frames[position]).dirty = false;
          }

          //set the valid bit to indicate it's out of memory
          pageTable.get(frames[position]).valid = false;

          //reset the frame bit because it's out of memory now
          pageTable.get(frames[position]).frame = -1;

          //update the physical memory
          frames[position] = address;

          //update the page table entry's physical address
          pte.frame = position;

        }

        //remove the finished reference
        ref.get(address).removeFirst();
      }

      //now this data is in the memory
      pte.valid = true;

      //update the page table
      pageTable.put(address,pte);

      //finally increase the number of memory accesses
      memoryAcc++;

      line = reader.readLine();

    }
    reader.close();

    //print stats
    System.out.println("\nAlgorithm: Opt"); //print stats
    System.out.println("Number of frames: " + nFrame);
    System.out.println("Total memory accesses: " + memoryAcc);
    System.out.println("Total page faults: " + pageFaults);
    System.out.println("Total writes to disk: " + writes);
  }




  /*****************************************************/
  //first in first out algorithm
  public static void fifo(int nFrame, String fileName) throws Exception{

    int pageFaults = 0;
    int memoryAcc = 0;
    int writes = 0;


    HashMap<Integer,PTE>  pageTable = new HashMap<Integer,PTE>();   //create the  page table
    LinkedList<Integer> ref = new LinkedList<Integer>();    //create the FIFO queue

    //declare the physical address
    int[] frames = new int[nFrame];


    //create a BufferedReader to read trace file line by line
    BufferedReader reader = new BufferedReader(new FileReader(fileName));

    //Initialize the page table
    int size = (int)Math.pow(2,20);    //we need 20 bits to store the whole 32 bit address
    for(int i=0; i < size; i++){
      PTE pte = new PTE(i);    //create a new default page entry
      pageTable.put(i,pte);
    }

    //intilize the physical frames
    for(int i=0; i< nFrame; i++){
      frames[i] = -1;
    }


    String line = reader.readLine();  //current line

    while(line != null){
      String[] array = line.split(" ");   //split input line
      for(int i =0; i<array.length;i++){
        array[i].toLowerCase();
      }
      int address = Integer.parseInt(array[1].substring(2,7),16);   //convert the address to decimal
      int offset = Integer.parseInt(array[1].substring(7,10),16);   //convert offset to decimal

      //set up a pte
      PTE pte = pageTable.get(address);   //get the pte from the page table
      pte.referenced = true;


      //check whether it is a "store" instruction
      if(array[0].equals("s")){
        pte.dirty = true;
      }

      //check if it is already in the memory
      if(pte.valid == true){
        //System.out.println(array[1] +  " hit");
      }

      else{
        //increase the number of page faults
        pageFaults++;
        //check if there's free frame
        boolean found = false;
        for(int i=0; i<nFrame; i++){

          if(frames[i] == -1){
            //This frame is available, we map a virtual page to it
            pte.frame = i;
            //store the address to the frame
            frames[i] = address;
            ref.add(i);
            found = true;
            //System.out.println(array[1] + " page fault - no eviction");
            break;
          }
        }

        //if no free frame, we need to evict the oldest one
        if(!found){
          //get the oldest frame number
          int position = ref.removeFirst();

          //get the page table entry of this frame
          PTE old = pageTable.get(frames[position]);

          //set the valid bit to indicate it's out of memory
          old.valid = false;

          //reset the frame bit because it's out of memory now
          old.frame = -1;

          //evict this frame
          if(old.dirty == false){
            //System.out.println(array[1] + " page fault - evict clean ");
            //writes++;   //we need to store the data to disk
          }

          else{
            //System.out.println(array[1] + " page fault - evict dirty ");
            writes++;   //we need to store the data to disk
            old.dirty = false;
          }

          //put the updated entry back
          pageTable.put(frames[position],old);

          //now we are ready to repalce the frame
          frames[position] = address;

          pte.frame = position;

          //add this position to LinkedList
          ref.add(position);
        }
        pte.referenced = true;
        pte.valid = true;
      }
      //now we update this new PageTable entry
      pageTable.put(address,pte);

      memoryAcc++;
      line = reader.readLine();
    }

    //close the buffer
    reader.close();


    //print stats
    System.out.println("\nAlgorithm: FIFO"); //print stats
    System.out.println("Number of frames: " + nFrame);
    System.out.println("Total memory accesses: " + memoryAcc);
    System.out.println("Total page faults: " + pageFaults);
    System.out.println("Total writes to disk: " + writes);
  }




  /*******************************************************/
  //Aging algorithm
  public static void age(int nFrame, String fileName, int refresh) throws Exception{
    int pageFaults = 0;
    int memoryAcc = 0;
    int writes = 0;

    //intilize the cpu cycles
    long cyclyes = -1;

    HashMap<Integer,PTE>  pageTable = new HashMap<Integer,PTE>();   //create the  page table
    //HashMap<Integer,Integer> counterTable = new HashMap<Integer,Integer>();  //create the counter table

    //declare the physical address
    int[] frames = new int[nFrame];


    //create a BufferedReader to read trace file line by line
    BufferedReader reader = new BufferedReader(new FileReader(fileName));

    //Initialize the page table and counter table
    int size = (int)Math.pow(2,20);    //we need 20 bits to store the whole 32 bit address
    for(int i=0; i < size; i++){
      PTE pte = new PTE(i);    //create a new default page entry
      pageTable.put(i,pte);
    }

    //intilize the physical frames
    for(int i=0; i< nFrame; i++){
      frames[i] = -1;
    }

    String line = reader.readLine();  //current line
    int cpu = 0;
    while(line != null){
      int R = (int)Math.pow(2,8);     //"referenced bit"
      //retrive elements
      String[] array = line.split(" ");   //split input line
      for(int i =0; i<array.length;i++){
        array[i].toLowerCase();
      }
      int address = Integer.parseInt(array[1].substring(2,7),16);   //convert the address to decimal
      //System.out.println("\nAddress is " + array[1]);
      int offset = Integer.parseInt(array[1].substring(7,10),16);   //convert offset to decimal
      int cycles = Integer.parseInt(array[2]);     //cycles since previous line
      int shifts = (cycles + cpu + 1) / refresh;      //how many shifts need to do since the previous line
      boolean shift = false;
      boolean found = false;
      //now we look at current entry
      PTE pte = pageTable.get(address);
      if(pte.valid != true){
        pte.first = true;
        pte.counter = "10000000";
      }



      //check the cpu cycle lapsed
      if(shifts > 0)
      {
        //System.out.println("Shift\n");
        shift = true;
        //now we have reached the refresh rate
        //shift all the page table
        for(int times = 0; times < shifts; times++){
          for(int i=0; i< nFrame;i++){
            if(frames[i] != -1){
              //This frame is nonempty and needed to be shift
              PTE entry = pageTable.get(frames[i]);
              String counter = entry.counter;   //ready to modify and shift
              char[] c = counter.toCharArray();
              for(int j=counter.length()-1; j>0; j--){
                c[j]=c[j-1];  //shifts all bits to right
              }

              //check it is been referenced last cycles
              if(entry.referenced){
                c[0]='1';    //make the first bit to 1
                entry.referenced = false;   //reset the referenced bit
              }
              else{
                c[0]='0';    //just leave it as 0
              }

              StringBuilder sb = new StringBuilder();
              for (char ch: c) {
                sb.append(ch);
              }

              entry.first = false;

              counter = sb.toString();
              entry.counter = counter;    //update the counter
              pageTable.put(frames[i],entry);     //update the page table entry
            }
          }
        }

      }
      if(shift){
        cpu = (cycles + cpu + 1) % refresh;
      }

      else{
        cpu = cpu + cycles + 1;
      }



      //check whether it is a "store" instruction
      if(array[0].equals("s")){
        pte.dirty = true;
      }

      //if it is in memory, this time is referenced --- hit
      if(pte.valid){
        pte.referenced = true;
        found = true;
      }

      else{
        //increase the number of page faults
        pageFaults++;
        //check if there's free frame
        for(int i=0; i<nFrame; i++){

          if(frames[i] == -1){
            //This frame is available, we map a virtual page to it
            pte.frame = i;
            //store the address to the frame
            frames[i] = address;
            found = true;
            //System.out.println(array[1] + " page fault - no eviction");
            break;
          }
        }
      }

      //if no free frame, we need to evict the lowest value
      if(!found){
        int position = 0;
        String min = pageTable.get(frames[position]).counter;
        for(int i=0;i<nFrame;i++){
          String cur = pageTable.get(frames[i]).counter;
          //convert to decimal and compare two counters
          if(Integer.parseInt(min,2) > Integer.parseInt(cur,2)){
            position = i;
            min = cur;
          }
          //if the counters are the same
          else if (Integer.parseInt(min,2) == Integer.parseInt(cur,2)){

            //if they are both dirty
            if(pageTable.get(frames[i]).dirty == pageTable.get(frames[i]).dirty ){
              //Compare the virtual address and choose the smaller one
              if(frames[position] > frames[i]){
                position = i;
                min = cur;
              }
            }

            //check if current pointer is clean
            else if(pageTable.get(frames[i]).dirty == false){
              //evict this frame
              position = i;
              min = cur;
            }
          }
        }
        //ready to modify old frame
        PTE old = pageTable.get(frames[position]);

        //set the valid bit to indicate it's out of memory
        old.valid = false;

        //reset the frame bit because it's out of memory now
        old.frame = -1;

        //evict this frame
        if(old.dirty){
          writes++;   //we need to store the data to disk
          old.dirty = false;
        }


        //reset the counter
        old.counter = "00000000";

        //put the updated entry back
        pageTable.put(frames[position],old);

        //now we are ready to repalce the frame
        frames[position] = address;
        pte.frame = position;
      }

      //now this address is in memory
      pte.valid = true;

      /*
      System.out.println("Now: ");
      for(int i=0;i<nFrame;i++){
        if(frames[i] != -1)
        System.out.println(Integer.toHexString(frames[i]) + " counter is: " + pageTable.get(frames[i]).counter);
      }*/

      //now we update this new PageTable entry
      pageTable.put(address,pte);
      memoryAcc++;
      line = reader.readLine();
    }
    //close the buffer
    reader.close();


    //print stats
    System.out.println("\nAlgorithm: Aging"); //print stats
    System.out.println("Number of frames: " + nFrame);
    System.out.println("Total memory accesses: " + memoryAcc);
    System.out.println("Total page faults: " + pageFaults);
    System.out.println("Total writes to disk: " + writes);

    for(int i=0;i<nFrame;i++){
      System.out.println(i + " counter is: " + pageTable.get(frames[i]).counter);
    }


  }





}
