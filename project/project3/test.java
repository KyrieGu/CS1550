public class test {

  public static void main (String args[]){

    String number = "00000000";
    char[] chars = number.toCharArray();
    for(int j=0;j<3;j++){

      for(int i=7; i>0; i--){
        chars[i] = chars[i-1];
      }

      chars[0] = '1';
      number = String.valueOf(chars);
      System.out.println(number);
    }
  }
}
