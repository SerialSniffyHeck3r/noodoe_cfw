#ifndef UI_NUMBER_H
#define UI_NUMBER_H

/* Presentation-only normalization of a writable, NUL-terminated unsigned
 * decimal string. Called after the owner formats its bounded numeric field.
 * Remove a zero only when another integer digit follows: 000 -> 0,
 * 000.1 -> 0.1 and 001.0 -> 1.0. Preserve fractional precision and explicit
 * '-' / '#' markers. Clock/date fields do not call this helper.
 * The in-place copy includes NUL and only shortens the input; it needs no
 * allocation, extra capacity, peripheral access or change to domain values.
 * Fixed label width, coordinates and alignment remain the view's concern. */
static inline void UiNumber_RemoveLeadingZeros(char *text)
{
    char *source=text;
    while(source[0]=='0'&&source[1]>='0'&&source[1]<='9')++source;
    if(source==text)return;
    while((*text++=*source++)!=0){}
}

#endif
